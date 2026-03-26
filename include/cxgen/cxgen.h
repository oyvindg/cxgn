/**
 * @file cxgen.h
 * @brief C API for cxgen - YAML to constexpr C++ code generator.
 *
 * Pure C interface for maximum portability and FFI compatibility.
 * Uses libyaml for YAML parsing.
 * Use cxgen.hpp for C++ RAII wrapper.
 */

#ifndef CXGEN_H
#define CXGEN_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Version
 * ═══════════════════════════════════════════════════════════════════════════ */

#define CXGEN_VERSION_MAJOR 1
#define CXGEN_VERSION_MINOR 0
#define CXGEN_VERSION_PATCH 0

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque handles
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct cg_struct_parser cg_struct_parser;
typedef struct cg_struct_info cg_struct_info;
typedef struct cg_field_info cg_field_info;
typedef struct cg_generator cg_generator;
typedef struct cg_path cg_path;
typedef struct cg_string_utils cg_string_utils;
typedef struct cg_output cg_output;

/* ═══════════════════════════════════════════════════════════════════════════
 * Error handling
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    CG_OK = 0,
    CG_ERR_FILE_NOT_FOUND,
    CG_ERR_PARSE_ERROR,
    CG_ERR_TYPE_MISMATCH,
    CG_ERR_MISSING_FIELD,
    CG_ERR_UNKNOWN_STRUCT,
    CG_ERR_UNKNOWN_TYPE,
    CG_ERR_YAML_ERROR,
    CG_ERR_OUT_OF_MEMORY,
    CG_ERR_EXPRESSION_ERROR
} cg_error_code;

typedef struct {
    cg_error_code code;
    const char* message;          /* Static or allocated, check needs_free */
    const char* path;             /* YAML path where error occurred */
    size_t line;
    size_t column;
    bool needs_free;              /* If true, caller must free message */
} cg_error;

/**
 * @brief Clear error and free allocated message if needed.
 * @param err Error to clear (NULL-safe)
 */
void cg_error_clear(cg_error* err);

/**
 * @brief Get human-readable string for error code.
 * @param code Error code
 * @return Static string description
 */
const char* cg_error_string(cg_error_code code);

/* ═══════════════════════════════════════════════════════════════════════════
 * String Utils API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new string utils instance.
 * @return String utils handle, or NULL on allocation failure
 */
cg_string_utils* cg_string_utils_new(void);

/**
 * @brief Free a string utils instance.
 * @param utils String utils to free (NULL-safe)
 */
void cg_string_utils_free(cg_string_utils* utils);

/**
 * @brief Convert string to snake_case.
 * @param utils String utils instance
 * @param s Input string
 * @return Newly allocated string (caller must free)
 */
char* cg_to_snake_case(const cg_string_utils* utils, const char* s);

/**
 * @brief Convert string to camelCase.
 * @param utils String utils instance
 * @param s Input string
 * @return Newly allocated string (caller must free)
 */
char* cg_to_camel_case(const cg_string_utils* utils, const char* s);

/**
 * @brief Convert string to PascalCase.
 * @param utils String utils instance
 * @param s Input string
 * @return Newly allocated string (caller must free)
 */
char* cg_to_pascal_case(const cg_string_utils* utils, const char* s);

/* ═══════════════════════════════════════════════════════════════════════════
 * Path API (YAML path tracking for error messages)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new path instance.
 * @return Path handle, or NULL on allocation failure
 */
cg_path* cg_path_new(void);

/**
 * @brief Free a path instance.
 * @param path Path to free (NULL-safe)
 */
void cg_path_free(cg_path* path);

/**
 * @brief Push a key segment onto the path.
 * @param path Path instance
 * @param key Key name to push
 */
void cg_path_push(cg_path* path, const char* key);

/**
 * @brief Push an array index onto the path.
 * @param path Path instance
 * @param index Array index to push
 */
void cg_path_push_index(cg_path* path, size_t index);

/**
 * @brief Pop the last segment from the path.
 * @param path Path instance
 */
void cg_path_pop(cg_path* path);

/**
 * @brief Get current path as string.
 * @param path Path instance
 * @return Newly allocated string (caller must free), e.g. "config.items[2].name"
 */
char* cg_path_to_string(const cg_path* path);

/**
 * @brief Compute a target path relative to the directory of another file path.
 * @param from_path Path whose containing directory is the base
 * @param target_path Path to express relative to @p from_path
 * @return Newly allocated relative path (caller must free)
 */
char* cg_path_relative_to_file(const char* from_path, const char* target_path);

/* ═══════════════════════════════════════════════════════════════════════════
 * Field Info API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get the type of a field.
 * @param field Field info
 * @return Type string (owned by field, do not free)
 */
const char* cg_field_get_type(const cg_field_info* field);

/**
 * @brief Get the name of a field.
 * @param field Field info
 * @return Name string (owned by field, do not free)
 */
const char* cg_field_get_name(const cg_field_info* field);

/**
 * @brief Get the default value of a field.
 * @param field Field info
 * @return Default value string or NULL if no default
 */
const char* cg_field_get_default(const cg_field_info* field);

/**
 * @brief Check if field is an array (Array<T>).
 * @param field Field info
 * @return true if array field
 */
bool cg_field_is_array(const cg_field_info* field);

/**
 * @brief Get array element type.
 * @param field Field info
 * @return Element type or NULL if not array
 */
const char* cg_field_get_array_element_type(const cg_field_info* field);

/**
 * @brief Check if field is optional (Optional<T>).
 * @param field Field info
 * @return true if optional field
 */
bool cg_field_is_optional(const cg_field_info* field);

/**
 * @brief Get optional value type.
 * @param field Field info
 * @return Value type or NULL if not optional
 */
const char* cg_field_get_optional_value_type(const cg_field_info* field);

/**
 * @brief Check if field is OneOf<A, B>.
 * @param field Field info
 * @return true if OneOf field
 */
bool cg_field_is_oneof(const cg_field_info* field);

/**
 * @brief Get first OneOf type.
 * @param field Field info
 * @return First type or NULL if not OneOf
 */
const char* cg_field_get_oneof_type_a(const cg_field_info* field);

/**
 * @brief Get second OneOf type.
 * @param field Field info
 * @return Second type or NULL if not OneOf
 */
const char* cg_field_get_oneof_type_b(const cg_field_info* field);

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct Info API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get the name of a struct.
 * @param info Struct info
 * @return Struct name (owned by info, do not free)
 */
const char* cg_struct_get_name(const cg_struct_info* info);

/**
 * @brief Get the file where struct is defined.
 * @param info Struct info
 * @return File path (owned by info, do not free)
 */
const char* cg_struct_get_defined_in(const cg_struct_info* info);

/**
 * @brief Get number of fields in struct.
 * @param info Struct info
 * @return Field count
 */
size_t cg_struct_get_field_count(const cg_struct_info* info);

/**
 * @brief Get field by index.
 * @param info Struct info
 * @param index Field index (0-based)
 * @return Field info or NULL if out of bounds
 */
const cg_field_info* cg_struct_get_field(const cg_struct_info* info, size_t index);

/**
 * @brief Find field by name.
 * @param info Struct info
 * @param name Field name
 * @return Field info or NULL if not found
 */
const cg_field_info* cg_struct_find_field(const cg_struct_info* info, const char* name);

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct Parser API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new struct parser.
 * @param utils String utils instance (borrowed, must outlive parser)
 * @return Parser handle, or NULL on allocation failure
 */
cg_struct_parser* cg_struct_parser_new(const cg_string_utils* utils);

/**
 * @brief Free a struct parser.
 * @param parser Parser to free (NULL-safe)
 */
void cg_struct_parser_free(cg_struct_parser* parser);

/**
 * @brief Parse header file and extract struct definitions.
 * @param parser Parser instance
 * @param header_path Path to C++ header file
 * @param err Error output (can be NULL)
 * @return true on success, false on error
 *
 * Follows #include directives recursively.
 */
bool cg_struct_parser_parse_file(cg_struct_parser* parser,
                                  const char* header_path, cg_error* err);

/**
 * @brief Get number of parsed structs.
 * @param parser Parser instance
 * @return Struct count
 */
size_t cg_struct_parser_get_struct_count(const cg_struct_parser* parser);

/**
 * @brief Get struct by index.
 * @param parser Parser instance
 * @param index Struct index (0-based)
 * @return Struct info or NULL if out of bounds
 */
const cg_struct_info* cg_struct_parser_get_struct(const cg_struct_parser* parser,
                                                   size_t index);

/**
 * @brief Find struct by name.
 * @param parser Parser instance
 * @param name Struct name
 * @return Struct info or NULL if not found
 */
const cg_struct_info* cg_struct_parser_find_struct(const cg_struct_parser* parser,
                                                    const char* name);

/**
 * @brief Check if type is a builtin type.
 * @param parser Parser instance
 * @param type Type name
 * @return true if builtin (int, double, bool, std::string, etc.)
 */
bool cg_struct_parser_is_builtin_type(const cg_struct_parser* parser, const char* type);

/**
 * @brief Check if type is constexpr-friendly.
 * @param parser Parser instance
 * @param type Type name
 * @return true if can be used in constexpr context
 */
bool cg_struct_parser_is_constexpr_friendly(const cg_struct_parser* parser, const char* type);

/* ═══════════════════════════════════════════════════════════════════════════
 * Expression Handler API (for injection of external parsers like cxpr)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Callback to check if a field type should be treated as expression.
 * @param field_type The C++ type of the field
 * @param userdata User-provided context
 * @return true if field contains an expression to be parsed
 */
typedef bool (*cg_is_expression_field_fn)(const char* field_type, void* userdata);

/**
 * @brief Callback to generate C++ code for an expression.
 * @param expression The expression string from YAML
 * @param yaml_path YAML path for error reporting
 * @param userdata User-provided context
 * @return Newly allocated C++ code string (caller frees), NULL on error
 */
typedef char* (*cg_generate_expression_fn)(const char* expression,
                                            const char* yaml_path,
                                            void* userdata);

/**
 * @brief Callback to validate expression syntax.
 * @param expression The expression string
 * @param yaml_path YAML path for error reporting
 * @param userdata User-provided context
 * @return NULL if valid, newly allocated error message otherwise (caller frees)
 */
typedef char* (*cg_validate_expression_fn)(const char* expression,
                                            const char* yaml_path,
                                            void* userdata);

/**
 * @brief Expression handler callbacks.
 */
typedef struct {
    cg_is_expression_field_fn is_expression_field;
    cg_generate_expression_fn generate_code;
    cg_validate_expression_fn validate;
    void* userdata;
} cg_expression_handler;

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
    const char* oneof_wrapper;             /**< Wrapper token for parsing oneof (default: "OneOf") */
    const char* array_ctor_fmt;            /**< Output ctor format (default: "Array<%s>{%s_data, %s_count}") */
    const char* optional_empty_fmt;        /**< Empty optional format (default: "Optional<%s>::empty()") */
    const char* optional_value_prefix_fmt; /**< Optional value prefix (default: "Optional<%s>{") */
    const char* optional_value_suffix;     /**< Optional value suffix (default: "}") */
} cg_type_options;

/* ═══════════════════════════════════════════════════════════════════════════
 * Code Generator API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new code generator.
 * @param parser Struct parser (borrowed, must outlive generator)
 * @param utils String utils (borrowed, must outlive generator)
 * @return Generator handle, or NULL on allocation failure
 */
cg_generator* cg_generator_new(const cg_struct_parser* parser,
                                const cg_string_utils* utils);

/**
 * @brief Free a code generator.
 * @param gen Generator to free (NULL-safe)
 */
void cg_generator_free(cg_generator* gen);

/**
 * @brief Set expression handler for expression fields.
 * @param gen Generator instance
 * @param handler Expression handler callbacks
 *
 * When set, fields with types that match is_expression_field()
 * will be processed by the handler instead of as raw strings.
 */
void cg_generator_set_expression_handler(cg_generator* gen,
                                          const cg_expression_handler* handler);

/**
 * @brief Set output type options for a generator.
 * @param gen Generator instance
 * @param options Custom options (NULL-safe; NULL keeps defaults)
 */
void cg_generator_set_type_options(cg_generator* gen, const cg_type_options* options);

/**
 * @brief Generate constexpr C++ code from YAML config.
 * @param gen Generator instance
 * @param yaml_path Path to YAML file
 * @param header_path Path to header file (for output include)
 * @param err Error output (can be NULL)
 * @return Output handle on success, NULL on error
 */
cg_output* cg_generate(cg_generator* gen, const char* yaml_path,
                        const char* header_path, cg_error* err);

/**
 * @brief Generate constexpr C++ code from YAML text in memory.
 * @param gen Generator instance
 * @param yaml_text YAML document text (UTF-8)
 * @param yaml_virtual_path Virtual/source path used in diagnostics
 * @param header_path Path to header file (for output include)
 * @param err Error output (can be NULL)
 * @return Output handle on success, NULL on error
 */
cg_output* cg_generate_from_yaml_text(cg_generator* gen,
                                      const char* yaml_text,
                                      const char* yaml_virtual_path,
                                      const char* header_path,
                                      cg_error* err);

/**
 * @brief Get generated code from output.
 * @param output Output handle
 * @return Code string (owned by output, do not free)
 */
const char* cg_output_get_code(const cg_output* output);

/**
 * @brief Get length of generated code.
 * @param output Output handle
 * @return Code length in bytes
 */
size_t cg_output_get_code_length(const cg_output* output);

/**
 * @brief Free output handle.
 * @param output Output to free (NULL-safe)
 */
void cg_output_free(cg_output* output);

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
} cg_cli_args;

/**
 * @brief Parse command line arguments.
 * @param argc Argument count
 * @param argv Argument values
 * @param args Output arguments
 * @param err Error output (can be NULL)
 * @return true on success, false on error
 */
bool cg_parse_args(int argc, char* argv[], cg_cli_args* args, cg_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CXGEN_H */
