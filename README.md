# cxgn

[![CI](https://github.com/oyvindg/cxgn/actions/workflows/ci.yml/badge.svg)](https://github.com/oyvindg/cxgn/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

`cxgn` is a build-time C11 library and CLI that reads C schema headers plus YAML and emits a generated `.gen.h` file.

Use it when configuration should be authored as YAML but consumed as ordinary C data: embedded defaults, simulation scenes, strategy registries, test fixtures, generated lookup tables, and other config that benefits from compile-time type checking with no runtime parser dependency.

The generated output is pure C11:

- include guards, not `#pragma once`
- `static const` objects and backing arrays
- C designated initializers
- `const char*` for strings
- cxgn array helpers for sequences and optional helpers for explicit presence checks
- nullable pointers for optional object references
- validation with configurable warning/error policy
- deterministic batch generation with keyed config maps

## Schema model

Schema headers are C headers. Use `const char*` for strings and declare array/optional helper types with `macros.h`.

```c
#include <cxgn/macros.h>

typedef struct Point2d {
    float x;
    float y;
} Point2d;

typedef struct SpawnArea {
    Point2d center;
    float radius;
} SpawnArea;

CXGN_ARRAY_TYPEDEF(Point2d, Point2dArray)
CXGN_OPTIONAL_TYPEDEF(Point2d, OptionalPoint2d)

typedef struct SceneConfig {
    const char* name;
    Point2dArray waypoints;
    const char* skybox;
    OptionalPoint2d previewPoint;
    const Point2d* pointTarget;
    const SpawnArea* spawnArea;
} SceneConfig;
```

Pointer fields and `const char*` strings are nullable. If a pointer/string field is missing or set to `null` in YAML, the generated initializer uses `0`. If an object pointer is present, `cxgn` emits a `static const` backing object and stores a pointer to it.

Use `CXGN_OPTIONAL_TYPEDEF` for values where `0` or `false` is a valid value and the C code must distinguish that from `null` or a missing YAML field.

## Maps (named entries)

`CXGN_MAP_TYPEDEF(T, Name)` binds a YAML **mapping with arbitrary keys** to a keyed array of `T`. This is for config authored as `name: value` entries (e.g. a strategy's named expressions) without listing each entry as an array element. Each `key: value` becomes one `T`: the key fills `T`'s **first field**, and the value fills the rest — a scalar value goes into the second field, an object value spreads into `T`'s remaining fields. The C representation is the same `{const T* data; size_t count;}` view as an array.

```c
typedef struct NamedExpr {
    const char* name;   /* first field receives the YAML key */
    const char* expr;   /* second field receives the scalar value */
} NamedExpr;

CXGN_MAP_TYPEDEF(NamedExpr, NamedExprMap)

typedef struct Strategy {
    const char* title;
    NamedExprMap expressions;
} Strategy;
```

```yaml
title: demo
expressions:           # arbitrary keys -> keyed array, no host pre-flattening
  trend: "close > ema"
  entry: "trend and rsi > 50"
```

generates:

```c
static NamedExpr const _backing_Strategy_expressions_0_data[] = {
    {.name = "trend", .expr = "close > ema"},
    {.name = "entry", .expr = "trend and rsi > 50"},
};
static const Strategy config = {
    .title = "demo",
    .expressions = {.data = _backing_Strategy_expressions_0_data, .count = 2},
};
```

```yaml
skybox: null
preview_point: null
spawn_area:
  center: {x: 8.0, y: 6.0}
  radius: 3.5
```

If a header contains multiple candidate root structs, `cxgn` normally generates from the last parsed struct. Library callers can override that explicitly:

```c
cxgn_generator_set_root_struct(gen, "SceneConfig");
```

This is useful for reusable schema headers that expose helper structs first and the desired generation root is not the final typedef in the file.

## In-memory document API

`cxgn` also exposes a small document builder/serializer for memory-only pipelines. This is intended for projects that need domain-specific normalization before generic `.gen.h` emission.

```c
cxgn_document* doc = cxgn_document_new("normalized.yaml");
cxgn_node* root = cxgn_node_new_object();
cxgn_node_object_append(root, "name", cxgn_node_new_string("alpha", 5), 1, 1);
cxgn_document_set_root(doc, root);

char* yaml_text = cxgn_document_to_yaml_text(doc);
cxgn_output* out = cxgn_generate_from_document(
    gen, doc, "normalized.yaml", "scene.h", NULL);
```

Use `cxgn_document_to_yaml_text()` when YAML text is useful for inspection or tests. Use `cxgn_generate_from_document()` when the whole normalization and generation pipeline should stay in memory.

## Ownership model

`cxgn` retains dependency handles internally:

- `cxgn_struct_parser_new()` retains `cxgn_string_utils`
- `cxgn_generator_new()` retains the parser and string utils
- `cxgn_batch_new()` retains the generator
- `cxgn_output_retain()` / `cxgn_output_free()` support shared ownership

Parent handles can be released after child construction. The API also exposes explicit retain/release helpers for shared ownership.

## Validation model

Default validation policy:

- unknown fields: warning
- duplicate keys: error
- missing non-optional fields: error

## CLI

```bash
cxgn --yaml scene.yaml --header scene.h --output scene.gen.h
```

```bash
cxgn --yaml scene.yaml --header scene.h --output scene.gen.h --strict
```

Use `--verbose` to print the files and generation steps being processed. Validation diagnostics include path/line/column where libyaml provides source locations.

Use `--strict` to promote validation warnings, such as unknown fields, to errors.

Batch mode activates automatically when `--yaml` is repeated or a glob resolves to multiple files:

```bash
cxgn --yaml "strategies/**/*.yaml" --header scene.h --output strategies.all.gen.h
cxgn --yaml macd.yaml --yaml rsi.yaml --header scene.h --output combined.all.gen.h
cxgn --yaml macd.yaml rsi.yaml --header scene.h --output combined.all.gen.h
```

Batch-specific options:

- [`--map-root <dir>`](examples/batch/strategies/alpha.yaml) derives keys from the path relative to that directory and errors if a file falls outside it
- [`--map-name <name>`](examples/batch/CMakeLists.txt) renames the emitted config registry variable
- [`--map-type <name>`](examples/batch/strategy.h) renames the emitted config map entry typedef

For the input files [examples/batch/strategies/alpha.yaml](examples/batch/strategies/alpha.yaml) and [examples/batch/strategies/beta.yaml](examples/batch/strategies/beta.yaml):

```yaml
# alpha.yaml
name: "alpha"
threshold: 10

# beta.yaml
name: "beta"
threshold: 20
```

Run:

```bash
cxgn --yaml "examples/batch/strategies/*.yaml" \
     --header examples/batch/strategy.h \
     --output strategies.all.gen.h \
     --map-root examples/batch \
     --map-type strategy_preset_entry_t
```

The relevant generated output is:

```c
/* -- Entry: strategies/alpha -- Source: examples/batch/strategies/alpha.yaml */
static const StrategyPreset strategies_alpha_config = {
    .name = "alpha",
    .threshold = 10
};

/* -- Entry: strategies/beta -- Source: examples/batch/strategies/beta.yaml */
static const StrategyPreset strategies_beta_config = {
    .name = "beta",
    .threshold = 20
};

typedef struct {
    const char* key;
    const StrategyPreset* config;
} strategy_preset_entry_t;

typedef struct Config {
    const strategy_preset_entry_t* entries;
    size_t count;
} Config;

static const strategy_preset_entry_t _config_entries[] = {
    {"strategies/alpha", &strategies_alpha_config},
    {"strategies/beta", &strategies_beta_config},
};

static const Config config = {
    .entries = _config_entries,
    .count = 2,
};
```

Here `--map-root examples/batch` keeps `strategies/alpha` and `strategies/beta` as map keys, and `--map-type strategy_preset_entry_t` controls the entry typedef. Add `--map-name strategy_presets` to name the final registry variable `strategy_presets`.

Library-level batch control:

```c
cxgn_batch_options options;
cxgn_batch_options_init(&options);
options.map_root = "strategies";
options.continue_on_error = true;

cxgn_batch_result result = {0};
if (cxgn_batch_generate_detailed(batch, "strategy.h", &options, &result, NULL)) {
    /* result.entries[i] exposes yaml_path/key/identifier/error per file */
}
cxgn_batch_result_clear(&result);
```

Optional shared helper header:

```bash
cxgn --yaml scene.yaml --header scene.h --output scene.gen.h \
     --helpers-header cxgn/macros.h
```

At runtime/API level:

```c
cxgn_validation_options validation;
cxgn_validation_options_init(&validation);
validation.strict_mode = true;
cxgn_generator_set_validation_options(gen, &validation);
```

Capability detection:

```c
if (!cxgn_has_yaml()) {
    /* Generator APIs will return CXGN_ERR_FEATURE_DISABLED */
}
```

## Generated output

The generated file includes:

- source comments
- an include guard derived from the output filename
- `#include <stddef.h>` and `#include <stdbool.h>`
- either inlined helper typedefs or `#include <...>` from `--helpers-header`
- the schema header
- backing arrays and a root `static const ... config`

In batch mode, the combined output also includes:

- per-entry prefixed symbols to avoid collisions
- a trailing keyed config registry with `.entries` and `.count`
- optional per-entry result metadata through `cxgn_batch_generate_detailed()`

Example field output:

```c
.name = "Level 1",
.waypoints = {.data = _backing_SceneConfig_waypoints_0_data, .count = 3},
.skybox = 0,
.previewPoint = {.has_value = false},
.spawnArea = &_backing_SceneConfig_spawnArea_1,
```

## Install and integration

CMake package export:

```cmake
find_package(cxgn CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE cxgn::cxgn)
```

pkg-config:

```bash
pkg-config --cflags --libs cxgn
```

Build/install:

```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix /usr/local
```

Disable YAML support explicitly:

```bash
cmake -S . -B build -DCXGN_ENABLE_YAML=OFF
```

## Examples

Repo examples live in:

- [`examples/minimal/`](examples/minimal/) for a single YAML to C header flow, with [`main.c`](examples/minimal/main.c) including [`scene.gen.h`](examples/minimal/scene.gen.h)
- [`examples/batch/`](examples/batch/) for multi-file registry generation, with [`main.c`](examples/batch/main.c) iterating the generated map in [`strategies.all.gen.h`](examples/batch/strategies.all.gen.h)

## Installed headers

`include/cxgn/` contains:

- `batch.h`
- `cxgn.h`
- `macros.h`
