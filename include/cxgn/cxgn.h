/**
 * @file cxgn.h
 * @brief C API for cxgn - YAML to constexpr C++ code generator.
 *
 * Pure C interface for maximum portability and FFI compatibility.
 * Uses libyaml for YAML parsing.
 * Use cxgn.hpp for C++ RAII wrapper.
 */

#ifndef CXGN_H
#define CXGN_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Version
 * ═══════════════════════════════════════════════════════════════════════════ */

#define CXGN_VERSION_MAJOR 1
#define CXGN_VERSION_MINOR 0
#define CXGN_VERSION_PATCH 0

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque handles
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct cxgn_struct_parser cxgn_struct_parser;
typedef struct cxgn_struct_info cxgn_struct_info;
typedef struct cxgn_field_info cxgn_field_info;
typedef struct cxgn_generator cxgn_generator;
typedef struct cxgn_path cxgn_path;
typedef struct cxgn_string_utils cxgn_string_utils;
typedef struct cxgn_output cxgn_output;

/* ═══════════════════════════════════════════════════════════════════════════
 * Error handling
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    CXGN_OK = 0,
    CXGN_ERR_FILE_NOT_FOUND,
    CXGN_ERR_PARSE_ERROR,
    CXGN_ERR_TYPE_MISMATCH,
    CXGN_ERR_MISSING_FIELD,
    CXGN_ERR_UNKNOWN_STRUCT,
    CXGN_ERR_UNKNOWN_TYPE,
    CXGN_ERR_YAML_ERROR,
    CXGN_ERR_OUT_OF_MEMORY,
    CXGN_ERR_EXPRESSION_ERROR
} cxgn_error_code;

typedef struct {
    cxgn_error_code code;
    const char* message;          /* Static or allocated, check needs_free */
    const char* path;             /* YAML path where error occurred */
    size_t line;
    size_t column;
    bool needs_free;              /* If true, caller must free message */
} cxgn_error;

/**
 * @brief Clear error and free allocated message if needed.
 * @param err Error to clear (NULL-safe)
 */
void cxgn_error_clear(cxgn_error* err);

/**
 * @brief Get human-readable string for error code.
 * @param code Error code
 * @return Static string description
 */
const char* cxgn_error_string(cxgn_error_code code);

/* ═══════════════════════════════════════════════════════════════════════════
 * String Utils API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new string utils instance.
 * @return String utils handle, or NULL on allocation failure
 */
cxgn_string_utils* cxgn_string_utils_new(void);

/**
 * @brief Free a string utils instance.
 * @param utils String utils to free (NULL-safe)
 */
void cxgn_string_utils_free(cxgn_string_utils* utils);

/**
 * @brief Convert string to snake_case.
 * @param utils String utils instance
 * @param s Input string
 * @return Newly allocated string (caller must free)
 */
char* cxgn_to_snake_case(const cxgn_string_utils* utils, const char* s);

/**
 * @brief Convert string to camelCase.
 * @param utils String utils instance
 * @param s Input string
 * @return Newly allocated string (caller must free)
 */
char* cxgn_to_camel_case(const cxgn_string_utils* utils, const char* s);

/**
 * @brief Convert string to PascalCase.
 * @param utils String utils instance
 * @param s Input string
 * @return Newly allocated string (caller must free)
 */
char* cxgn_to_pascal_case(const cxgn_string_utils* utils, const char* s);

/* ═══════════════════════════════════════════════════════════════════════════
 * Path API (YAML path tracking for error messages)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new path instance.
 * @return Path handle, or NULL on allocation failure
 */
cxgn_path* cxgn_path_new(void);

/**
 * @brief Free a path instance.
 * @param path Path to free (NULL-safe)
 */
void cxgn_path_free(cxgn_path* path);

/**
 * @brief Push a key segment onto the path.
 * @param path Path instance
 * @param key Key name to push
 */
void cxgn_path_push(cxgn_path* path, const char* key);

/**
 * @brief Push an array index onto the path.
 * @param path Path instance
 * @param index Array index to push
 */
void cxgn_path_push_index(cxgn_path* path, size_t index);

/**
 * @brief Pop the last segment from the path.
 * @param path Path instance
 */
void cxgn_path_pop(cxgn_path* path);

/**
 * @brief Get current path as string.
 * @param path Path instance
 * @return Newly allocated string (caller must free), e.g. "config.items[2].name"
 */
char* cxgn_path_to_string(const cxgn_path* path);

/**
 * @brief Compute a target path relative to the directory of another file path.
 * @param from_path Path whose containing directory is the base
 * @param target_path Path to express relative to @p from_path
 * @return Newly allocated relative path (caller must free)
 */
char* cxgn_path_relative_to_file(const char* from_path, const char* target_path);

/* ═══════════════════════════════════════════════════════════════════════════
 * Field Info API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get the type of a field.
 * @param field Field info
 * @return Type string (owned by field, do not free)
 */
const char* cxgn_field_get_type(const cxgn_field_info* field);

/**
 * @brief Get the name of a field.
 * @param field Field info
 * @return Name string (owned by field, do not free)
 */
const char* cxgn_field_get_name(const cxgn_field_info* field);

/**
 * @brief Get the default value of a field.
 * @param field Field info
 * @return Default value string or NULL if no default
 */
const char* cxgn_field_get_default(const cxgn_field_info* field);

/**
 * @brief Check if field is an array (Array<T>).
 * @param field Field info
 * @return true if array field
 */
bool cxgn_field_is_array(const cxgn_field_info* field);

/**
 * @brief Get array element type.
 * @param field Field info
 * @return Element type or NULL if not array
 */
const char* cxgn_field_get_array_element_type(const cxgn_field_info* field);

/**
 * @brief Check if field is optional (Optional<T>).
 * @param field Field info
 * @return true if optional field
 */
bool cxgn_field_is_optional(const cxgn_field_info* field);

/**
 * @brief Get optional value type.
 * @param field Field info
 * @return Value type or NULL if not optional
 */
const char* cxgn_field_get_optional_value_type(const cxgn_field_info* field);

/**
 * @brief Check if field is std::variant<T...>.
 * @param field Field info
 * @return true if std::variant field
 */
bool cxgn_field_is_variant(const cxgn_field_info* field);

/**
 * @brief Get number of types in std::variant.
 * @param field Field info
 * @return Type count, or 0 if not std::variant
 */
size_t cxgn_field_get_variant_type_count(const cxgn_field_info* field);

/**
 * @brief Get a type from std::variant by index.
 * @param field Field info
 * @param index Type index (0-based)
 * @return Type string or NULL if out of range
 */
const char* cxgn_field_get_variant_type(const cxgn_field_info* field, size_t index);

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct Info API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get the name of a struct.
 * @param info Struct info
 * @return Struct name (owned by info, do not free)
 */
const char* cxgn_struct_get_name(const cxgn_struct_info* info);

/**
 * @brief Get the file where struct is defined.
 * @param info Struct info
 * @return File path (owned by info, do not free)
 */
const char* cxgn_struct_get_defined_in(const cxgn_struct_info* info);

/**
 * @brief Get number of fields in struct.
 * @param info Struct info
 * @return Field count
 */
size_t cxgn_struct_get_field_count(const cxgn_struct_info* info);

/**
 * @brief Get field by index.
 * @param info Struct info
 * @param index Field index (0-based)
 * @return Field info or NULL if out of bounds
 */
const cxgn_field_info* cxgn_struct_get_field(const cxgn_struct_info* info, size_t index);

/**
 * @brief Find field by name.
 * @param info Struct info
 * @param name Field name
 * @return Field info or NULL if not found
 */
const cxgn_field_info* cxgn_struct_find_field(const cxgn_struct_info* info, const char* name);

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct Parser API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new struct parser.
 * @param utils String utils instance (borrowed, must outlive parser)
 * @return Parser handle, or NULL on allocation failure
 */
cxgn_struct_parser* cxgn_struct_parser_new(const cxgn_string_utils* utils);

/**
 * @brief Free a struct parser.
 * @param parser Parser to free (NULL-safe)
 */
void cxgn_struct_parser_free(cxgn_struct_parser* parser);

/**
 * @brief Parse header file and extract struct definitions.
 * @param parser Parser instance
 * @param header_path Path to C++ header file
 * @param err Error output (can be NULL)
 * @return true on success, false on error
 *
 * Follows #include directives recursively.
 */
bool cxgn_struct_parser_parse_file(cxgn_struct_parser* parser,
                                  const char* header_path, cxgn_error* err);

/**
 * @brief Get number of parsed structs.
 * @param parser Parser instance
 * @return Struct count
 */
size_t cxgn_struct_parser_get_struct_count(const cxgn_struct_parser* parser);

/**
 * @brief Get struct by index.
 * @param parser Parser instance
 * @param index Struct index (0-based)
 * @return Struct info or NULL if out of bounds
 */
const cxgn_struct_info* cxgn_struct_parser_get_struct(const cxgn_struct_parser* parser,
                                                   size_t index);

/**
 * @brief Find struct by name.
 * @param parser Parser instance
 * @param name Struct name
 * @return Struct info or NULL if not found
 */
const cxgn_struct_info* cxgn_struct_parser_find_struct(const cxgn_struct_parser* parser,
                                                    const char* name);

/**
 * @brief Check if type is a builtin type.
 * @param parser Parser instance
 * @param type Type name
 * @return true if builtin (int, double, bool, std::string, etc.)
 */
bool cxgn_struct_parser_is_builtin_type(const cxgn_struct_parser* parser, const char* type);

/**
 * @brief Check if type is constexpr-friendly.
 * @param parser Parser instance
 * @param type Type name
 * @return true if can be used in constexpr context
 */
bool cxgn_struct_parser_is_constexpr_friendly(const cxgn_struct_parser* parser, const char* type);

/* ═══════════════════════════════════════════════════════════════════════════
 * Expression Handler API (for injection of external parsers like cxpr)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Callback to check if a field type should be treated as expression.
 * @param field_type The C++ type of the field
 * @param userdata User-provided context
 * @return true if field contains an expression to be parsed
 */
typedef bool (*cxgn_is_expression_field_fn)(const char* field_type, void* userdata);

/**
 * @brief Callback to generate C++ code for an expression.
 * @param expression The expression string from YAML
 * @param yaml_path YAML path for error reporting
 * @param userdata User-provided context
 * @return Newly allocated C++ code string (caller frees), NULL on error
 */
typedef char* (*cxgn_generate_expression_fn)(const char* expression,
                                            const char* yaml_path,
                                            void* userdata);

/**
 * @brief Callback to validate expression syntax.
 * @param expression The expression string
 * @param yaml_path YAML path for error reporting
 * @param userdata User-provided context
 * @return NULL if valid, newly allocated error message otherwise (caller frees)
 */
typedef char* (*cxgn_validate_expression_fn)(const char* expression,
                                            const char* yaml_path,
                                            void* userdata);

/**
 * @brief Expression handler callbacks.
 */
typedef struct {
    cxgn_is_expression_field_fn is_expression_field;
    cxgn_generate_expression_fn generate_code;
    cxgn_validate_expression_fn validate;
    void* userdata;
} cxgn_expression_handler;

/* ═══════════════════════════════════════════════════════════════════════════
 * Type Options API (output syntax customization)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Type and constructor formatting options for generated output.
 *
 * All fields are optional; NULL keeps existing generator default.
 *
 * Format placeholders:
 * - array_ctor_fmt: `%s` = element_type, data_symbol, count_symbol
 * - optional_empty_fmt: `%s` = value_type
 * - optional_value_prefix_fmt: `%s` = value_type
 */
typedef struct {
    const char* array_wrapper;             /**< Wrapper token for parsing arrays (default: "Array") */
    const char* optional_wrapper;          /**< Wrapper token for parsing optionals (default: "Optional") */
    const char* variant_wrapper;           /**< Wrapper token for parsing std::variant (default: "std::variant") */
    const char* array_ctor_fmt;            /**< Output ctor format (default: "Array<%s>{%s_data, %s_count}") */
    const char* optional_empty_fmt;        /**< Empty optional format (default: "Optional<%s>::empty()") */
    const char* optional_value_prefix_fmt; /**< Optional value prefix (default: "Optional<%s>{") */
    const char* optional_value_suffix;     /**< Optional value suffix (default: "}") */
} cxgn_type_options;

/* ═══════════════════════════════════════════════════════════════════════════
 * C++ Standard Target
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Target C++ standard for generated output.
 *
 * Controls whether the generator may emit constructs that require
 * a specific C++ standard (e.g. constexpr std::string requires C++20).
 */
typedef enum {
    CXGN_CPP_STD_AUTO = 0, /**< Emit #if __cplusplus guards; works with any standard */
    CXGN_CPP_STD_17   = 17, /**< Target C++17 */
    CXGN_CPP_STD_20   = 20, /**< Target C++20 (default) */
} cxgn_cpp_std;

/* ═══════════════════════════════════════════════════════════════════════════
 * Code Generator API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new code generator.
 * @param parser Struct parser (borrowed, must outlive generator)
 * @param utils String utils (borrowed, must outlive generator)
 * @return Generator handle, or NULL on allocation failure
 */
cxgn_generator* cxgn_generator_new(const cxgn_struct_parser* parser,
                                const cxgn_string_utils* utils);

/**
 * @brief Free a code generator.
 * @param gen Generator to free (NULL-safe)
 */
void cxgn_generator_free(cxgn_generator* gen);

/**
 * @brief Set expression handler for expression fields.
 * @param gen Generator instance
 * @param handler Expression handler callbacks
 *
 * When set, fields with types that match is_expression_field()
 * will be processed by the handler instead of as raw strings.
 */
void cxgn_generator_set_expression_handler(cxgn_generator* gen,
                                          const cxgn_expression_handler* handler);

/**
 * @brief Set output type options for a generator.
 * @param gen Generator instance
 * @param options Custom options (NULL-safe; NULL keeps defaults)
 */
void cxgn_generator_set_type_options(cxgn_generator* gen, const cxgn_type_options* options);

/**
 * @brief Set the target C++ standard for generated output.
 * @param gen Generator instance
 * @param std Target standard (default: CXGN_CPP_STD_20)
 *
 * Affects which qualifiers are used in generated declarations:
 * - CXGN_CPP_STD_17: non-literal types (e.g. std::string) use `static const`
 *   backing storage and `const` config variable.
 * - CXGN_CPP_STD_20: all backing and config are `constexpr`.
 */
void cxgn_generator_set_cpp_std(cxgn_generator* gen, cxgn_cpp_std std);

/**
 * @brief Generate constexpr C++ code from YAML config.
 * @param gen Generator instance
 * @param yaml_path Path to YAML file
 * @param header_path Path to header file (for output include)
 * @param err Error output (can be NULL)
 * @return Output handle on success, NULL on error
 */
cxgn_output* cxgn_generate(cxgn_generator* gen, const char* yaml_path,
                        const char* header_path, cxgn_error* err);

/**
 * @brief Generate constexpr C++ code from YAML text in memory.
 * @param gen Generator instance
 * @param yaml_text YAML document text (UTF-8)
 * @param yaml_virtual_path Virtual/source path used in diagnostics
 * @param header_path Path to header file (for output include)
 * @param err Error output (can be NULL)
 * @return Output handle on success, NULL on error
 */
cxgn_output* cxgn_generate_from_yaml_text(cxgn_generator* gen,
                                      const char* yaml_text,
                                      const char* yaml_virtual_path,
                                      const char* header_path,
                                      cxgn_error* err);

/**
 * @brief Get generated code from output.
 * @param output Output handle
 * @return Code string (owned by output, do not free)
 */
const char* cxgn_output_get_code(const cxgn_output* output);

/**
 * @brief Get length of generated code.
 * @param output Output handle
 * @return Code length in bytes
 */
size_t cxgn_output_get_code_length(const cxgn_output* output);

/**
 * @brief Free output handle.
 * @param output Output to free (NULL-safe)
 */
void cxgn_output_free(cxgn_output* output);

/* ═══════════════════════════════════════════════════════════════════════════
 * CLI Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CLI arguments structure.
 */
typedef struct {
    const char* yaml_path;
    const char* header_path;
    const char* output_path;
    bool verbose;
    cxgn_cpp_std cpp_std; /**< Target C++ standard (default: CXGN_CPP_STD_20) */
} cxgn_cli_args;

/**
 * @brief Parse command line arguments.
 * @param argc Argument count
 * @param argv Argument values
 * @param args Output arguments
 * @param err Error output (can be NULL)
 * @return true on success, false on error
 */
bool cxgn_parse_args(int argc, char* argv[], cxgn_cli_args* args, cxgn_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CXGN_H */
