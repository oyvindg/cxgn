# cxgen

`cxgen` is a **build-time** C library and CLI. It reads C++ struct definitions and a YAML config file, and generates a `constexpr` root configuration object as a `.gen.hpp` header.

**cxgen runs during your build, not at runtime.** The YAML is parsed once by the generator, not by your application. The output is baked into your binary as a `constexpr` value — zero runtime overhead, no `libyaml` dependency in your final executable.

## Quick start

**1. Define your struct** (`Config.hpp`):

```cpp
#pragma once
#include <string>

struct Config {
    int    timeout;
    std::string name;
    bool   enabled = true;
};
```

**2. Write your YAML** (`config.yaml`):

```yaml
timeout: 42
name: "my-service"
enabled: true
```

**3. Generate** (`config.gen.hpp`):

```bash
cxgen --yaml config.yaml --header Config.hpp --output config.gen.hpp
```

**4. Include and use**:

```cpp
#include "config.gen.hpp"

static_assert(config.timeout == 42);
static_assert(config.enabled == true);

int main() {
    // config is constexpr — zero runtime cost
    serve(config.name, config.timeout);
}
```

The generated file looks like this:

```cpp
#pragma once
#include "Config.hpp"

constexpr Config config = {
    42,     // timeout
    "my-service",  // name
    true    // enabled
};
```

## Build-time integration

The typical workflow is to run `cxgen` as a custom build step so the generated header is always in sync with the YAML and struct:

```cmake
add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/config.gen.hpp
    COMMAND cxgen
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

CMake will re-run `cxgen` automatically whenever `config.yaml` or `Config.hpp` changes. Your application links against `my_config` and includes `config.gen.hpp` — it has no dependency on `libyaml` or any YAML parser at runtime.

## What cxgen supports

### C++ header parsing

`cxgen` parses C++ headers and extracts struct metadata for code generation:

- Multiple `struct` definitions in the same header
- Recursive `#include` following for relative includes
- Plain fields such as `int timeout;`
- Default field values such as `bool enabled = true;`
- Multi-declarations such as `float x, y, z;`
- Nested struct references where one field type is another parsed struct
- Wrapper field detection for `Array<T>`, `Optional<T>`, and `OneOf<A, B>`
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
| scalar or mapping   | `OneOf<A, B>{std::in_place_index<N>, …}`|

`OneOf<A, B>` selection is shape-based: a scalar picks the non-struct alternative (index 0), a mapping picks the struct alternative (index 1).

### Naming

YAML keys may match either the C++ field name directly or its `snake_case` form. A field named `maxRetryCount` can be set with `max_retry_count` in YAML.

The string utility API also handles `camelCase`, `PascalCase`, and acronym/digit-heavy names like `HTTPServer`, `getUserID`, and `i32Val`.

### Arrays, optionals, and tagged unions

The included wrapper headers are part of the supported model:

- `Array<T>` — non-owning, `constexpr`-friendly array view backed by generated static storage
- `Optional<T>` — `constexpr`-friendly optional with `empty()` support
- `OneOf<A, B>` — shape-based tagged union, emitted with `std::in_place_index<N>`

For arrays, cxgen emits backing storage in an anonymous namespace before the root config object.

### Defaults, missing values, and extra YAML keys

When YAML does not provide a field:

- If the C++ field has a default value, that value is used.
- If the field is `Optional<T>`, `Optional<T>::empty()` is emitted.
- Otherwise the field falls back to `{}` initialization.

These cases produce warnings on `stderr`, including ANSI color output and source references for both the YAML file and the header.

Warnings also fire for YAML keys that do not exist in the target struct.

### Error handling

The public C API exposes structured error information through `cg_error`:

- error code, message, YAML path, line, column

Error categories include: file not found, parse errors, type mismatch, missing required field, unknown struct, unknown type, YAML errors, out of memory, and expression errors.

### In-memory generation

In addition to reading YAML from disk, cxgen can generate code from YAML text in memory with `cg_generate_from_yaml_text(...)`. A virtual source path can be supplied for diagnostics.

### Output customization

The generator supports overriding wrapper tokens and emitted constructor syntax via `cg_type_options`.

This allows setups such as:

- parsing `Vec<T>` instead of `Array<T>`
- parsing `Maybe<T>` instead of `Optional<T>`
- keeping or renaming `OneOf<A, B>`
- customizing emitted constructors for arrays and optionals

### Expression hooks

cxgen can delegate selected field types to an external expression system through `cg_expression_handler`.

The hook API lets you provide callbacks to:

- decide whether a field type should be treated as an expression
- validate expression text
- generate C++ code for the expression

## Generated output model

The generator emits:

- static backing arrays in an anonymous namespace when arrays are present
- a root object named `config`
- code of the form `constexpr <RootStruct> config = …;`

The root struct is the last struct parsed from the target header/include graph, so config headers should place the intended top-level struct last.

## All-types example

The bundled `tests/fixtures/all_types` fixture exercises every supported type in one round trip.

**Header** (`tests/fixtures/all_types.hpp`) — abbreviated:

```cpp
#include <cxgen/Array.hpp>
#include <cxgen/Optional.hpp>
#include <cxgen/OneOf.hpp>
#include <string>
#include <string_view>
#include <cstdint>

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

    OneOf<int, Point2D> oneOfScalar;   // scalar → index 0
    OneOf<int, Point2D> oneOfMapping;  // mapping → index 1

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
str_val:   "hello cxgen"

int_array: [1, 2, 3, 4, 5]
str_array: ["alpha", "beta", "gamma"]

opt_int_present: 99
opt_int_absent:  null

one_of_scalar:  7
one_of_mapping:
  x: 10
  y: 20

nested:
  x: 3
  y: 4
```

**Generated output** (`all_types.gen.hpp`):

```cpp
#pragma once
#include "tests/fixtures/all_types.hpp"

namespace {
static constexpr int _backing_0_data[] = {1, 2, 3, 4, 5};
static constexpr size_t _backing_0_count = 5;
static constexpr std::string _backing_2_data[] = {"alpha", "beta", "gamma"};
static constexpr size_t _backing_2_count = 3;
} // namespace

constexpr AllTypesConfig config = {
    42,                                              // intVal
    9000000000000,                                   // i64Val
    3.14,                                            // floatVal
    2.71828182845,                                   // doubleVal
    true,                                            // boolTrue
    "hello cxgen",                                   // strVal
    Array<int>{_backing_0_data, _backing_0_count},   // intArray
    Array<std::string>{_backing_2_data, _backing_2_count}, // strArray
    Optional<int>{99},                               // optIntPresent
    Optional<int>::empty(),                          // optIntAbsent
    OneOf<int, Point2D>{std::in_place_index<0>, 7},  // oneOfScalar
    OneOf<int, Point2D>{std::in_place_index<1>, Point2D{10, 20}}, // oneOfMapping
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

static_assert(config.oneOfScalar.index() == 0);   // holds int
static_assert(config.oneOfMapping.index() == 1);  // holds Point2D

static_assert(config.nested.x == 3);
static_assert(config.nested.y == 4);

int main() {
    // Access at runtime — still zero-cost, values are baked into the binary
    for (int v : config.intArray) { /* 1 2 3 4 5 */ }
    if (config.optIntPresent)  { use(*config.optIntPresent); }
}
```

## Build and test

`cxgen` uses CMake and requires `libyaml` for YAML-backed generation.

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

Use this when you want `cxgen` installed globally under `/usr/local/bin`, `/usr/local/lib`, and `/usr/local/include`.

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
cxgen --yaml <file> --header <file> --output <file> [--verbose]
```

Options:

| Flag              | Short | Description                               |
| ----------------- | ----- | ----------------------------------------- |
| `--yaml`          | `-y`  | YAML configuration file                   |
| `--header`        | `-h`  | C++ header file with struct definitions   |
| `--output`        | `-o`  | Output file for generated code            |
| `--verbose`       | `-v`  | Enable verbose output                     |
| `--help`          |       | Show usage                                |

Example:

```bash
cxgen --yaml config.yaml --header Config.hpp --output config.gen.hpp
```

Using the bundled all-types fixture:

```bash
cxgen \
  --yaml tests/fixtures/all_types.yaml \
  --header tests/fixtures/all_types.hpp \
  --output all_types.gen.hpp
```

The CLI writes a generated header containing a `#pragma once`, an `#include` of the source header, and the generated body.

To compile code that includes your generated header, add the cxgen public headers to the include path:

```bash
c++ -Iinclude -I. -c all_types.gen.hpp
```

If you installed cxgen with `cmake --install`, use the install prefix's `include/` directory instead.

## C API example

```c
#include <cxgen/cxgen.h>

int main(void) {
    cg_error err = {0};

    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);

    if (!cg_struct_parser_parse_file(parser, "Config.hpp", &err)) {
        fprintf(stderr, "parse error: %s\n", err.message);
        return 1;
    }

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* out = cg_generate(gen, "config.yaml", "Config.hpp", &err);
    if (!out) {
        fprintf(stderr, "generate error: %s\n", err.message);
        return 1;
    }

    printf("%s\n", cg_output_get_code(out));

    cg_output_free(out);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    return 0;
}
```

## Repository layout

| Path                        | Contents                                                  |
| --------------------------- | --------------------------------------------------------- |
| `include/cxgen/cxgen.h`     | Public C API                                              |
| `include/cxgen/Array.hpp`   | `constexpr`-friendly array view                          |
| `include/cxgen/Optional.hpp`| `constexpr`-friendly optional                            |
| `include/cxgen/OneOf.hpp`   | Shape-based tagged union                                  |
| `src/`                      | Parser, generator, utilities, CLI                         |
| `tests/`                    | Coverage for parsing, generation, naming, warnings, types |

## License

See [`LICENSE`](LICENSE).

## Scope and limitations

cxgen is best suited to configuration-oriented C++ structs with simple field declarations. It does not aim to be a full C++ parser. The features described here reflect what is covered by the current parser and tests — not arbitrary C++ syntax.
