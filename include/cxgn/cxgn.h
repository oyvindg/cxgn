/**
 * @file cxgn.h
 * @brief C API for cxgn - YAML to generated C headers.
 *
 * Pure C interface for maximum portability and FFI compatibility.
 * Uses libyaml for YAML parsing.
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
#define CXGN_VERSION_PATCH 2

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
typedef struct cxgn_document cxgn_document;
typedef struct cxgn_node cxgn_node;

typedef enum {
    CXGN_NODE_NULL = 0,
    CXGN_NODE_BOOL,
    CXGN_NODE_INTEGER,
    CXGN_NODE_FLOAT,
    CXGN_NODE_STRING,
    CXGN_NODE_ARRAY,
    CXGN_NODE_OBJECT
} cxgn_node_type;

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
    CXGN_ERR_EXPRESSION_ERROR,
    CXGN_ERR_DUPLICATE_KEY,
    CXGN_ERR_UNKNOWN_FIELD,
    CXGN_ERR_FEATURE_DISABLED,
    CXGN_ERR_UNSUPPORTED_TYPE
} cxgn_error_code;

typedef struct cxgn_error {
    cxgn_error_code code;
    const char* message;          /* Static or allocated, check needs_free */
    const char* path;             /* YAML path where error occurred */
    size_t line;
    size_t column;
    bool needs_free;              /* If true, caller must free message */
} cxgn_error;

/**
 * @brief Clear error and free allocated diagnostic strings if needed.
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
 * Capability Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Report whether YAML-backed generation support is available.
 * @return true when libyaml-backed generation APIs are enabled in this build
 */
bool cxgn_has_yaml(void);

/* ═══════════════════════════════════════════════════════════════════════════
 * YAML Document API
 * ═══════════════════════════════════════════════════════════════════════════ */

cxgn_document* cxgn_document_from_yaml_file(const char* yaml_path, cxgn_error* err);
cxgn_document* cxgn_document_from_yaml_text(const char* yaml_text,
                                            const char* source_name,
                                            cxgn_error* err);
cxgn_document* cxgn_document_new(const char* source_name);
bool cxgn_document_set_root(cxgn_document* doc, cxgn_node* root);
void cxgn_document_free(cxgn_document* doc);
const cxgn_node* cxgn_document_get_root(const cxgn_document* doc);
const char* cxgn_document_get_source_name(const cxgn_document* doc);
char* cxgn_document_to_yaml_text(const cxgn_document* doc);

cxgn_node_type cxgn_node_get_type(const cxgn_node* node);
size_t cxgn_node_get_line(const cxgn_node* node);
size_t cxgn_node_get_column(const cxgn_node* node);
const char* cxgn_node_get_raw_scalar_text(const cxgn_node* node, size_t* len);

cxgn_node* cxgn_node_new_null(void);
cxgn_node* cxgn_node_new_bool(bool value);
cxgn_node* cxgn_node_new_integer(long long value);
cxgn_node* cxgn_node_new_float(double value);
cxgn_node* cxgn_node_new_string(const char* data, size_t len);
cxgn_node* cxgn_node_new_array(void);
cxgn_node* cxgn_node_new_object(void);
void cxgn_node_free(cxgn_node* node);
void cxgn_node_set_location(cxgn_node* node, size_t line, size_t column);
bool cxgn_node_set_raw_scalar_text(cxgn_node* node, const char* text, size_t len);
bool cxgn_node_array_append(cxgn_node* array_node, cxgn_node* item);
bool cxgn_node_object_append(cxgn_node* object_node,
                             const char* key,
                             cxgn_node* value,
                             size_t line,
                             size_t column);

bool cxgn_node_get_bool(const cxgn_node* node, bool* out_value);
bool cxgn_node_get_integer(const cxgn_node* node, long long* out_value);
bool cxgn_node_get_float(const cxgn_node* node, double* out_value);
const char* cxgn_node_get_string(const cxgn_node* node, size_t* len);

size_t cxgn_node_array_count(const cxgn_node* node);
const cxgn_node* cxgn_node_array_at(const cxgn_node* node, size_t index);

size_t cxgn_node_object_count(const cxgn_node* node);
const char* cxgn_node_object_key_at(const cxgn_node* node, size_t index);
const cxgn_node* cxgn_node_object_value_at(const cxgn_node* node, size_t index);
const cxgn_node* cxgn_node_object_find(const cxgn_node* node, const char* key, size_t ordinal);

cxgn_document* cxgn_document_clone(const cxgn_document* doc);
cxgn_document* cxgn_document_merge(const cxgn_document* base_doc, const cxgn_document* overlay_doc);
cxgn_document* cxgn_document_merge_many(const cxgn_document* const* docs, size_t count);

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
 * @param utils String utils to release (NULL-safe)
 */
void cxgn_string_utils_free(cxgn_string_utils* utils);

/**
 * @brief Retain a string utils instance for shared ownership.
 * @param utils String utils instance
 * @return The same pointer for chaining
 */
cxgn_string_utils* cxgn_string_utils_retain(cxgn_string_utils* utils);

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
 * @brief Check if field is a cxgn array typedef.
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
 * @brief Check if field is a cxgn optional typedef.
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
 * @param utils String utils instance (retained internally)
 * @return Parser handle, or NULL on allocation failure
 */
cxgn_struct_parser* cxgn_struct_parser_new(const cxgn_string_utils* utils);

/**
 * @brief Free a struct parser.
 * @param parser Parser to release (NULL-safe)
 */
void cxgn_struct_parser_free(cxgn_struct_parser* parser);

/**
 * @brief Retain a parser for shared ownership.
 * @param parser Parser instance
 * @return The same pointer for chaining
 */
cxgn_struct_parser* cxgn_struct_parser_retain(cxgn_struct_parser* parser);

/**
 * @brief Parse header file and extract C struct definitions.
 * @param parser Parser instance
 * @param header_path Path to C header file
 * @param err Error output (can be NULL)
 * @return true on success, false on error
 *
 * Follows #include directives recursively.
 */
bool cxgn_struct_parser_parse_file(cxgn_struct_parser* parser,
                                  const char* header_path, cxgn_error* err);

/**
 * @brief Parse header text from memory and extract C struct definitions.
 * @param parser Parser instance
 * @param header_text Header source text
 * @param source_name Virtual/source path used for diagnostics and relative includes
 * @param err Error output (can be NULL)
 * @return true on success, false on error
 */
bool cxgn_struct_parser_parse_text(cxgn_struct_parser* parser,
                                   const char* header_text,
                                   const char* source_name,
                                   cxgn_error* err);

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
 * @return true if builtin (int, double, bool, const char*, etc.)
 */
bool cxgn_struct_parser_is_builtin_type(const cxgn_struct_parser* parser, const char* type);

/**
 * @brief Legacy helper kept for API compatibility.
 * @param parser Parser instance
 * @param type Type name
 * @return true for plain scalar/builtin C types
 */
bool cxgn_struct_parser_is_constexpr_friendly(const cxgn_struct_parser* parser, const char* type);

/* ═══════════════════════════════════════════════════════════════════════════
 * Expression Handler API (for injection of external parsers like cxpr)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Callback to check if a field type should be treated as expression.
 * @param field_type The C type of the field
 * @param userdata User-provided context
 * @return true if field contains an expression to be parsed
 */
typedef bool (*cxgn_is_expression_field_fn)(const char* field_type, void* userdata);

/**
 * @brief Callback to generate C code for an expression.
 * @param expression The expression string from YAML
 * @param yaml_path YAML path for error reporting
 * @param userdata User-provided context
 * @return Newly allocated C initializer snippet (caller frees), NULL on error.
 *
 * The returned code is inserted directly into a generated struct initializer.
 * It must therefore be valid pure-C expression syntax for an initializer
 * context, for example a quoted string literal or a C struct literal.
 * It must not emit declarations, statements, or C++-only syntax.
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
 *
 * When provided, validation runs before generate_code(). If validation fails,
 * generation aborts with CXGN_ERR_EXPRESSION_ERROR and generate_code() is not called.
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
 * Validation Policy API
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    CXGN_VALIDATION_IGNORE = 0,
    CXGN_VALIDATION_WARN,
    CXGN_VALIDATION_ERROR
} cxgn_validation_action;

typedef enum {
    CXGN_DIAGNOSTIC_WARNING = 1,
    CXGN_DIAGNOSTIC_ERROR = 2
} cxgn_diagnostic_level;

typedef void (*cxgn_diagnostic_fn)(cxgn_diagnostic_level level,
                                   const cxgn_error* diagnostic,
                                   void* userdata);

typedef struct {
    bool strict_mode;
    cxgn_validation_action unknown_field;
    cxgn_validation_action duplicate_key;
    cxgn_validation_action missing_field;
    cxgn_diagnostic_fn diagnostic_fn;
    void* diagnostic_userdata;
} cxgn_validation_options;

/**
 * @brief Initialize validation options with cxgn defaults.
 *
 * Default policy:
 * - unknown fields: warning
 * - duplicate keys: error
 * - missing required fields: error
 * - strict_mode: false
 * - diagnostic callback: NULL
 *
 * @param options Output options struct
 */
void cxgn_validation_options_init(cxgn_validation_options* options);

/* ═══════════════════════════════════════════════════════════════════════════
 * Type Options API (output syntax customization)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Legacy type option hooks kept for API compatibility.
 *
 * Pure-C output ignores these fields. They remain in the ABI so older callers
 * still compile against the C11 generator.
 *
 * Format placeholders:
 * - array_ctor_fmt: `%s` = element_type, data_symbol, count_symbol
 * - optional_empty_fmt: `%s` = value_type
 * - optional_value_prefix_fmt: `%s` = value_type
 */
typedef struct {
    const char* array_wrapper;
    const char* optional_wrapper;
    const char* array_ctor_fmt;
    const char* optional_empty_fmt;
    const char* optional_value_prefix_fmt;
    const char* optional_value_suffix;
} cxgn_type_options;

/* ═══════════════════════════════════════════════════════════════════════════
 * Legacy Output Mode
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Legacy output-mode enum kept for API compatibility.
 */
typedef enum {
    CXGN_CPP_STD_AUTO = 0,
    CXGN_CPP_STD_17   = 17,
    CXGN_CPP_STD_20   = 20,
} cxgn_cpp_std;

/* ═══════════════════════════════════════════════════════════════════════════
 * Code Generator API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new code generator.
 * @param parser Struct parser (retained internally, must not be NULL)
 * @param utils Optional string utils handle. When NULL, the parser's retained
 *        string utils instance is reused.
 * @return Generator handle, or NULL on allocation failure
 */
cxgn_generator* cxgn_generator_new(const cxgn_struct_parser* parser,
                                const cxgn_string_utils* utils);

/**
 * @brief Free a code generator.
 * @param gen Generator to release (NULL-safe)
 */
void cxgn_generator_free(cxgn_generator* gen);

/**
 * @brief Retain a generator for shared ownership.
 * @param gen Generator instance
 * @return The same pointer for chaining
 */
cxgn_generator* cxgn_generator_retain(cxgn_generator* gen);

/**
 * @brief Set expression handler for expression fields.
 * @param gen Generator instance
 * @param handler Expression handler callbacks
 *
 * When set, fields with types that match is_expression_field()
 * will be processed by the handler instead of as raw strings. Handlers must
 * emit code that is valid in a pure-C initializer context.
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
 * @brief Set validation options for a generator.
 * @param gen Generator instance
 * @param options Validation options copied into the generator
 */
void cxgn_generator_set_validation_options(cxgn_generator* gen,
                                           const cxgn_validation_options* options);

/**
 * @brief Convenience setter for strict validation mode.
 * @param gen Generator instance
 * @param strict When true, validation warnings such as unknown fields become errors
 */
void cxgn_generator_set_strict_mode(cxgn_generator* gen, bool strict);

/**
 * @brief Legacy setter kept for API compatibility.
 * @param gen Generator instance
 * @param std Legacy target mode value
 *
 * Pure-C output ignores this setting and always emits the same C11 form.
 */
void cxgn_generator_set_cpp_std(cxgn_generator* gen, cxgn_cpp_std std);

/**
 * @brief Override the helper header included by generated output.
 * @param gen Generator instance
 * @param helpers_header Header path emitted as `#include <...>`, or NULL to inline typedefs
 */
void cxgn_generator_set_helpers_header(cxgn_generator* gen, const char* helpers_header);

/**
 * @brief Override which parsed struct is used as the generation root.
 * @param gen Generator instance
 * @param root_struct_name Struct typedef/tag name, or NULL to use the last parsed struct
 */
void cxgn_generator_set_root_struct(cxgn_generator* gen, const char* root_struct_name);

/**
 * @brief Generate C code from YAML config.
 * @param gen Generator instance
 * @param yaml_path Path to YAML file
 * @param header_path Path to header file (for output include)
 * @param err Error output (can be NULL)
 * @return Output handle on success, NULL on error
 */
cxgn_output* cxgn_generate(cxgn_generator* gen, const char* yaml_path,
                        const char* header_path, cxgn_error* err);

/**
 * @brief Generate C code directly from an in-memory YAML document tree.
 * @param gen Generator instance
 * @param doc Parsed/constructed document tree
 * @param yaml_virtual_path Virtual/source path used in diagnostics
 * @param header_path Path to header file (for output include)
 * @param err Error output (can be NULL)
 * @return Output handle on success, NULL on error
 */
cxgn_output* cxgn_generate_from_document(cxgn_generator* gen,
                                         const cxgn_document* doc,
                                         const char* yaml_virtual_path,
                                         const char* header_path,
                                         cxgn_error* err);

/**
 * @brief Generate C code from YAML text in memory.
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
 * @param output Output to release (NULL-safe)
 */
void cxgn_output_free(cxgn_output* output);

/**
 * @brief Retain an output handle for shared ownership.
 * @param output Output handle
 * @return The same pointer for chaining
 */
cxgn_output* cxgn_output_retain(cxgn_output* output);

#include <cxgn/batch.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CLI Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CLI arguments structure.
 */
typedef struct {
    const char* yaml_path;
    const char* header_path;
    const char* header_include;
    const char* output_path;
    const char* helpers_header;
    const char* root_struct;
    bool strict;
    bool verbose;
    cxgn_cpp_std cpp_std; /**< Legacy field kept for compatibility */
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
