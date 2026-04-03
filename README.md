# cxgn

[![CI](https://github.com/oyvindg/cxgn/actions/workflows/ci.yml/badge.svg)](https://github.com/oyvindg/cxgn/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

`cxgn` is a **build-time** C library and CLI. It reads C++ struct definitions and a YAML config file, and generates a root configuration object as a `.gen.hpp` header.

**cxgn runs during your build, not at runtime.** The YAML is parsed once by the generator, not by your application. With the default `--std 20`, the output is baked into your binary as a `constexpr` value — zero runtime overhead, no `libyaml` dependency in your final executable.

## Quick start

`cxgn` works best when your C++ structs are the schema. You define the config model in a header, write values in YAML, and `cxgn` emits a `constexpr` initializer with no runtime YAML dependency.

**1. Define your structs** (`scene.hpp`):

```cpp
#pragma once
#include <cxgn/Array.hpp>
#include <cxgn/Optional.hpp>
#include <string>

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Point2d {
    float x;
    float y;
};

struct SceneConfig {
    std::string name;
    Vec3 background;
    Array<Point2d> waypoints;
    Optional<std::string> skybox;
    int maxObjects = 100;
};
```

**2. Write your YAML** (`scene.yaml`):

```yaml
name: "Level 1"
background:
  x: 0.1
  y: 0.2
  z: 0.4
waypoints:
  - { x: 0.0,  y: 0.0 }
  - { x: 10.0, y: 5.0 }
  - { x: 20.0, y: 0.0 }
# skybox omitted -> Optional, defaults to empty
max_objects: 256
```

**3. Generate** (`scene.gen.hpp`):

```bash
cxgn --yaml scene.yaml --header scene.hpp --output scene.gen.hpp
```

The CLI also supports `--std 17`, `--std 20`, or `--std auto`, plus `--verbose` for progress output.

**4. Include and use**:

```cpp
#include "scene.gen.hpp"

static_assert(config.maxObjects == 256);
static_assert(config.waypoints.size() == 3);
static_assert(config.background.z == 0.4f);

int main() {
    // config is generated at build time
    return config.maxObjects == 256 ? 0 : 1;
}
```

The generated file looks like this:

```cpp
// GENERATED FILE - DO NOT EDIT
// Source YAML: scene.yaml
// Source Header: scene.hpp
#pragma once
#include <variant>
#include "scene.hpp"

namespace {
static constexpr Point2d waypoints_data[] = {
    {0.0f, 0.0f}, {10.0f, 5.0f}, {20.0f, 0.0f}
};
static constexpr Array<Point2d> waypoints_arr = {waypoints_data, 3};
} // namespace

constexpr SceneConfig config = {
    .name = "Level 1",
    .background = {0.1f, 0.2f, 0.4f},
    .waypoints = waypoints_arr,
    .skybox = Optional<std::string>::empty(),
    .maxObjects = 256
};
```

## Build-time integration

The typical workflow is to run `cxgn` as a custom build step so the generated header is always in sync with the YAML and struct:

```cmake
add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/config.gen.hpp
    COMMAND cxgn
            --yaml   ${CMAKE_CURRENT_SOURCE_DIR}/config.yaml
            --header ${CMAKE_CURRENT_SOURCE_DIR}/Config.hpp
            --output ${CMAKE_CURRENT_BINARY_DIR}/config.gen.hpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/config.yaml
            ${CMAKE_CURRENT_SOURCE_DIR}/Config.hpp
    COMMENT "Generating config.gen.hpp"
)

add_library(my_config INTERFACE)
target_sources(my_config INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/config.gen.hpp)
target_include_directories(my_config INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
```

CMake will re-run `cxgn` automatically whenever `config.yaml` or `Config.hpp` changes. Your application links against `my_config` and includes `config.gen.hpp` — it has no dependency on `libyaml` or any YAML parser at runtime.

## What cxgn supports

### C++ header parsing

`cxgn` parses C++ headers and extracts struct metadata for code generation:

- Multiple `struct` definitions in the same header
- Recursive `#include` following for relative includes
- Plain fields such as `int timeout;`
- Default field values such as `bool enabled = true;`
- Multi-declarations such as `float x, y, z;`
- Nested struct references where one field type is another parsed struct
- Wrapper field detection for `Array<T>`, `Optional<T>`, and `std::variant<T...>`
- Builtin type recognition for all standard integer, float, bool, and string types:
  - `int`, `unsigned int`, `short`, `unsigned short`
  - `long`, `unsigned long`, `long long`, `unsigned long long`
  - `char`, `signed char`, `unsigned char`
  - `float`, `double`, `long double`
  - `bool`, `size_t`
  - `int8_t` … `int64_t`, `uint8_t` … `uint64_t`
  - `std::string`, `std::string_view`

The parser is intentionally lightweight. It is designed for config-style structs, not full C++ language coverage.

### YAML to C++ generation

The generator maps YAML values onto the parsed struct model and emits C++ initialization code.

| YAML shape          | C++ output                              |
| ------------------- | --------------------------------------- |
| integer             | integer fields; floats get `.0` suffix  |
| float               | `float`, `double`, `long double`        |
| boolean             | `bool`                                  |
| string              | `std::string`, `std::string_view`       |
| `null`              | `Optional<T>::empty()`                  |
| scalar              | `Optional<T>{value}`                    |
| sequence            | `Array<T>{backing, count}`              |
| mapping             | nested struct initializer               |
| scalar or mapping   | `std::variant<T...>{std::in_place_index<N>, …}`|

`std::variant<T...>` selection is shape-based: a scalar picks the first non-struct alternative, a mapping picks the first struct alternative.

### Naming

YAML keys may match either the C++ field name directly or its `snake_case` form. A field named `maxRetryCount` can be set with `max_retry_count` in YAML.

The string utility API also handles `camelCase`, `PascalCase`, and acronym/digit-heavy names like `HTTPServer`, `getUserID`, and `i32Val`.

### Arrays, optionals, and tagged unions

The included wrapper headers are part of the supported model:

- `Array<T>` — non-owning, `constexpr`-friendly array view backed by generated static storage
- `Optional<T>` — `constexpr`-friendly optional with `empty()` support
- `std::variant<T...>` — shape-based tagged union, emitted with `std::in_place_index<N>`

For arrays, cxgn emits backing storage in an anonymous namespace before the root config object.

### Defaults, missing values, and extra YAML keys

When YAML does not provide a field:

- If the C++ field has a default value, that value is used.
- If the field is `Optional<T>`, `Optional<T>::empty()` is emitted.
- Otherwise the field falls back to `{}` initialization.

These cases produce warnings on `stderr`, including ANSI color output and source references for both the YAML file and the header.

Warnings also fire for YAML keys that do not exist in the target struct.

### Error handling

The public C API exposes structured error information through `cxgn_error`:

- error code, message, YAML path, line, column

Error categories include: file not found, parse errors, type mismatch, missing required field, unknown struct, unknown type, YAML errors, out of memory, and expression errors.

### In-memory generation

In addition to reading YAML from disk, cxgn can generate code from YAML text in memory with `cxgn_generate_from_yaml_text(...)`. A virtual source path can be supplied for diagnostics.

### Output customization

The generator supports overriding wrapper tokens and emitted constructor syntax via `cxgn_type_options`.

This allows setups such as:

- parsing `Vec<T>` instead of `Array<T>`
- parsing `Maybe<T>` instead of `Optional<T>`
- customizing the `std::variant` wrapper token
- customizing emitted constructors for arrays and optionals

### Expression hooks

cxgn can delegate selected field types to an external expression system through `cxgn_expression_handler`.

The hook API lets you provide callbacks to:

- decide whether a field type should be treated as an expression
- validate expression text
- generate C++ code for the expression

## Generated output model

The generator emits:

- static backing arrays in an anonymous namespace when arrays are present
- a root object named `config`
- code of the form `<qualifier> <RootStruct> config = …;`

The qualifier depends on the selected target standard:

- `--std 20` emits `constexpr` for the root object when possible
- `--std 17` emits `const` for non-literal cases such as `std::string`
- `--std auto` emits `#if __cplusplus` guards so the generated header works in both modes

The root struct is the last struct parsed from the target header/include graph, so config headers should place the intended top-level struct last.

## All-types example

The bundled `tests/fixtures/all_types` fixture exercises every supported type in one round trip.

**Header** (`tests/fixtures/all_types.hpp`) — abbreviated:

```cpp
#include <cxgn/Array.hpp>
#include <cxgn/Optional.hpp>
#include <variant>
#include <string>
#include <string_view>
#include <cstdint>
#include "shapes.hpp"

struct Point2D { int x; int y; };

struct AllTypesConfig {
    int          intVal;
    int64_t      i64Val;
    float        floatVal;
    double       doubleVal;
    bool         boolTrue;
    std::string  strVal;

    Array<int>          intArray;
    Array<std::string>  strArray;

    Optional<int>       optIntPresent;
    Optional<int>       optIntAbsent;

    std::variant<int, Point2D> variantScalar;   // scalar → index 0
    std::variant<int, Point2D> variantMapping;  // mapping → index 1
    std::variant<Circle, Rectangle> shape;      // mapping → first struct alternative

    Point2D nested;
};
```

**YAML** (`tests/fixtures/all_types.yaml`) — abbreviated:

```yaml
int_val:   42
i64_val:   9000000000000
float_val: 3.14
double_val: 2.71828182845
bool_true:  true
str_val:   "hello cxgn"

int_array: [1, 2, 3, 4, 5]
str_array: ["alpha", "beta", "gamma"]

opt_int_present: 99
opt_int_absent:  null

variant_scalar:  7
variant_mapping:
  x: 10
  y: 20

shape:
  radius: 12.5

nested:
  x: 3
  y: 4
```

**Generated output** (`all_types.gen.hpp`):

```cpp
// GENERATED FILE - DO NOT EDIT
// Source YAML: tests/fixtures/all_types.yaml
// Source Header: tests/fixtures/all_types.hpp
#pragma once
#include <variant>
#include "tests/fixtures/all_types.hpp"

namespace {
static constexpr char _spool_0[] = "hello cxgn";
static constexpr char _spool_1[] = "alpha";
static constexpr char _spool_2[] = "beta";
static constexpr char _spool_3[] = "gamma";
static constexpr int _backing_AllTypesConfig_intArray_data[] = {1, 2, 3, 4, 5};
static constexpr std::string _backing_AllTypesConfig_strArray_data[] = {_spool_1, _spool_2, _spool_3};
} // namespace

constexpr AllTypesConfig config = {
    42,                                              // intVal
    9000000000000,                                   // i64Val
    3.14,                                            // floatVal
    2.71828182845,                                   // doubleVal
    true,                                            // boolTrue
    _spool_0,                                        // strVal
    Array<int>{_backing_AllTypesConfig_intArray_data, 5},   // intArray
    Array<std::string>{_backing_AllTypesConfig_strArray_data, 3}, // strArray
    Optional<int>{99},                               // optIntPresent
    Optional<int>::empty(),                          // optIntAbsent
    std::variant<int, Point2D>{std::in_place_index<0>, 7},  // variantScalar
    std::variant<int, Point2D>{std::in_place_index<1>, Point2D{10, 20}}, // variantMapping
    std::variant<Circle, Rectangle>{std::in_place_index<0>, Circle{12.5}}, // shape
    {3, 4}                                           // nested
};
```

**Using the generated config in C++** (the YAML has already been consumed at build time):

```cpp
#include "all_types.gen.hpp"

// Verified at compile time — the values are constants in the binary
static_assert(config.intVal == 42);
static_assert(config.i64Val == 9000000000000LL);
static_assert(config.boolTrue == true);

static_assert(config.intArray[0] == 1);
static_assert(config.intArray.size() == 5);

static_assert(config.optIntPresent);
static_assert(*config.optIntPresent == 99);
static_assert(!config.optIntAbsent);

static_assert(config.variantScalar.index() == 0);   // holds int
static_assert(config.variantMapping.index() == 1);  // holds Point2D
static_assert(config.shape.index() == 0);           // holds Circle

static_assert(config.nested.x == 3);
static_assert(config.nested.y == 4);

int main() {
    // Access at runtime — still zero-cost, values are baked into the binary
    for (int v : config.intArray) { /* 1 2 3 4 5 */ }
    if (config.optIntPresent)  { use(*config.optIntPresent); }
}
```

## Build and test

`cxgn` uses CMake and requires `libyaml` for YAML-backed generation.

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

Or all in one line:

```bash
cmake -B build && cmake --build build && ctest --test-dir build
```

If `libyaml` is not available, the project still builds the non-YAML parts of the library, but code generation is disabled.

### Installation

By default, `cmake --install build` uses CMake's standard system prefix, typically `/usr/local` on Linux. That means a system-wide install usually needs `sudo`:

```bash
sudo cmake --install build
```

Use this when you want `cxgn` installed globally under `/usr/local/bin`, `/usr/local/lib`, and `/usr/local/include`.

If you see a permission error such as `Permission denied` while installing to `/usr/local`, either rerun the install command with `sudo` or use a user-local prefix instead.

For a user-local install:

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
export PATH="$HOME/.local/bin:$PATH"
```

This installs under `~/.local` and does not require `sudo`.

If you previously ran `sudo cmake --install build` and then switch to a non-root install, you may need to remove or `chown` `build/install_manifest.txt` if it was left owned by `root`.

Add the `export PATH=…` line to your shell profile (`~/.bashrc`, `~/.zshrc`) to make it permanent.

## CLI usage

```text
cxgn --yaml <file> --header <file> --output <file> [--std <ver>] [--verbose]
```

Options:

| Flag              | Short | Description                               |
| ----------------- | ----- | ----------------------------------------- |
| `--yaml`          | `-y`  | YAML configuration file                   |
| `--header`        | `-h`  | C++ header file with struct definitions   |
| `--output`        | `-o`  | Output file for generated code            |
| `--std`           |       | Target C++ standard: `17`, `20`, or `auto` |
| `--verbose`       | `-v`  | Enable verbose output                     |
| `--help`          |       | Show usage                                |

Example:

```bash
cxgn --yaml config.yaml --header Config.hpp --output config.gen.hpp
```

Examples with standard selection:

```bash
cxgn --yaml config.yaml --header Config.hpp --output config.gen.hpp --std 17
cxgn --yaml config.yaml --header Config.hpp --output config.gen.hpp --std auto --verbose
```

Using the bundled all-types fixture:

```bash
cxgn \
  --yaml tests/fixtures/all_types.yaml \
  --header tests/fixtures/all_types.hpp \
  --output all_types.gen.hpp
```

The CLI writes a generated header containing:

- file banner comments with the YAML and header source paths
- `#pragma once`
- `#include <variant>`
- an `#include` of the source header, rewritten relative to the output file
- the generated body from `cxgn_generate(...)`

To compile code that includes your generated header, add the cxgn public headers to the include path:

```bash
c++ -Iinclude -I. -c all_types.gen.hpp
```

If you installed cxgn with `cmake --install`, use the install prefix's `include/` directory instead.

## C API example

```c
#include <cxgn/cxgn.h>

int main(void) {
    cxgn_error err = {0};

    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);

    if (!cxgn_struct_parser_parse_file(parser, "Config.hpp", &err)) {
        fprintf(stderr, "parse error: %s\n", err.message);
        return 1;
    }

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate(gen, "config.yaml", "Config.hpp", &err);
    if (!out) {
        fprintf(stderr, "generate error: %s\n", err.message);
        return 1;
    }

    printf("%s\n", cxgn_output_get_code(out));

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    return 0;
}
```

## Repository layout

| Path                        | Contents                                                  |
| --------------------------- | --------------------------------------------------------- |
| `include/cxgn/cxgn.h`     | Public C API                                              |
| `include/cxgn/Array.hpp`   | `constexpr`-friendly array view                          |
| `include/cxgn/Optional.hpp`| `constexpr`-friendly optional                            |
| `include/cxgn/Array.hpp` and `Optional.hpp` | Constexpr-friendly wrappers                                  |
| `src/`                      | Parser, generator, utilities, CLI                         |
| `tests/`                    | Coverage for parsing, generation, naming, warnings, types |

## License

See [`LICENSE`](LICENSE).

## Scope and limitations

cxgn is best suited to configuration-oriented C++ structs with simple field declarations. It does not aim to be a full C++ parser. The features described here reflect what is covered by the current parser and tests — not arbitrary C++ syntax.
