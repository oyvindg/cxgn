# cxgn

`cxgn` is a build-time C library and CLI that reads C schema headers plus YAML and emits a generated `.gen.h` file.

The generated output is pure C11:

- include guards, not `#pragma once`
- `static const` objects and backing arrays
- C designated initializers
- `const char*` for strings
- cxgn array/optional helper structs for sequences and nullable values

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

## CLI

```bash
cxgn --yaml scene.yaml --header scene.h --output scene.gen.h
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

## Installed headers

`include/cxgn/` now contains:

- `batch.h`
- `cxgn.h`
- `macros.h`
