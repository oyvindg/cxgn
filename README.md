# cxgn

`cxgn` is a build-time C11 library and CLI that reads C schema headers plus YAML and emits a generated `.gen.h` file.

The generated output is pure C11:

- include guards, not `#pragma once`
- `static const` objects and backing arrays
- C designated initializers
- `const char*` for strings
- cxgn array/optional helper structs for sequences and nullable values
- validation with configurable warning/error policy
- deterministic batch generation with keyed config maps

`cxgn` is now C11-first. Legacy C++ compatibility fields remain as no-op ABI shims, but generated output and documented usage are pure C.

## Schema model

Schema headers are C headers. Use `const char*` for strings and declare array/optional helper types with `macros.h`.

```c
#include <cxgn/macros.h>

typedef struct Point2d {
    float x;
    float y;
} Point2d;

CXGN_ARRAY_TYPEDEF(Point2d, cxgn_array_Point2d_t)
CXGN_OPTIONAL_TYPEDEF(const char*, cxgn_optional_const_char_ptr_t)

typedef struct SceneConfig {
    const char* name;
    cxgn_array_Point2d_t waypoints;
    cxgn_optional_const_char_ptr_t skybox;
} SceneConfig;
```

## Ownership model

`cxgn` now retains dependency handles internally:

- `cxgn_struct_parser_new()` retains `cxgn_string_utils`
- `cxgn_generator_new()` retains the parser and string utils
- `cxgn_batch_new()` retains the generator
- `cxgn_output_retain()` / `cxgn_output_free()` support shared ownership

Callers no longer need to keep parent objects alive manually after child construction. The API also exposes explicit retain/release helpers for shared ownership.

## Validation model

Default validation policy:

- unknown fields: warning
- duplicate keys: error
- missing non-optional fields: warning with zero-initialized fallback

Enable strict mode in the CLI:

## CLI

```bash
cxgn --yaml scene.yaml --header scene.h --output scene.gen.h
```

```bash
cxgn --yaml scene.yaml --header scene.h --output scene.gen.h --strict
```

Batch mode activates automatically when `--yaml` is repeated or a glob resolves to multiple files:

```bash
cxgn --yaml "strategies/**/*.yaml" --header scene.h --output strategies.all.gen.h
cxgn --yaml macd.yaml --yaml rsi.yaml --header scene.h --output combined.all.gen.h
cxgn --yaml macd.yaml rsi.yaml --header scene.h --output combined.all.gen.h
```

Batch-specific options:

- `--map-root <dir>` derives keys from the path relative to that directory and errors if a file falls outside it
- `--map-name <name>` renames the emitted config map array
- `--map-type <name>` renames the emitted config map entry typedef

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
- the schema header
- either inlined helper typedefs or `#include <...>` from `--helpers-header`
- backing arrays and a root `static const ... config`

In batch mode, the combined output also includes:

- per-entry prefixed symbols to avoid collisions
- a trailing keyed config map array plus `<map_name>_count`

Example field output:

```c
.name = "Level 1",
.waypoints = {.data = _backing_SceneConfig_waypoints_0_data, .count = 3},
.skybox = {.has_value = false},
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

- `examples/minimal/` for a single YAML to C header flow
- `examples/batch/` for multi-file registry generation

## Installed headers

`include/cxgn/` now contains:

- `batch.h`
- `cxgn.h`
- `macros.h`
