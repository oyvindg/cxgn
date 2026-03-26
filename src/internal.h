/**
 * @file internal.h
 * @brief Internal structures and declarations for cxgen.
 *
 * This file defines the concrete types behind the opaque handles
 * declared in cxgen.h. It is NOT part of the public API.
 */

#ifndef CXGEN_INTERNAL_H
#define CXGEN_INTERNAL_H

#include <cxgen/cxgen.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define CG_MAX_FIELDS 64
#define CG_MAX_STRUCTS 64
#define CG_MAX_PATH_DEPTH 32
#define CG_MAX_INCLUDE_DEPTH 16
#define CG_BUFFER_SIZE 8192
#define CG_LINE_SIZE 1024

/* ═══════════════════════════════════════════════════════════════════════════
 * String Utils
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief String utilities instance.
 *
 * Currently stateless but allows for future caching or customization.
 */
struct cg_string_utils {
    int placeholder;  /* Empty structs not allowed in C */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Path (YAML path tracking)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Path segment type.
 */
typedef enum {
    CG_PATH_KEY,      /**< Named key segment */
    CG_PATH_INDEX     /**< Array index segment */
} cg_path_type;

/**
 * @brief A single path segment.
 */
typedef struct {
    cg_path_type type;
    union {
        char* key;        /**< For CG_PATH_KEY */
        size_t index;     /**< For CG_PATH_INDEX */
    } data;
} cg_path_segment;

/**
 * @brief YAML path tracking for error messages.
 */
struct cg_path {
    cg_path_segment segments[CG_MAX_PATH_DEPTH];
    size_t depth;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Field Info
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Information about a struct field.
 */
struct cg_field_info {
    char* name;               /**< Field name (owned) */
    char* type;               /**< Full type string (owned) */
    char* default_value;      /**< Default value or NULL (owned) */
    bool is_array;            /**< true if Array<T> */
    char* array_elem_type;    /**< Element type if array (owned) */
    bool is_optional;         /**< true if Optional<T> */
    char* optional_value_type; /**< Value type if optional (owned) */
    bool is_oneof;            /**< true if OneOf<A, B> */
    char* oneof_type_a;       /**< First type if OneOf (owned) */
    char* oneof_type_b;       /**< Second type if OneOf (owned) */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct Info
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Information about a parsed struct.
 */
struct cg_struct_info {
    char* name;               /**< Struct name (owned) */
    char* defined_in;         /**< File path (owned) */
    cg_field_info* fields;    /**< Array of fields (owned) */
    size_t field_count;
    size_t field_capacity;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct Parser
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief C++ header parser for struct definitions.
 */
struct cg_struct_parser {
    const cg_string_utils* utils;   /**< Borrowed reference */
    cg_struct_info* structs;        /**< Array of parsed structs (owned) */
    size_t struct_count;
    size_t struct_capacity;
    char** parsed_files;            /**< Already parsed files (owned) */
    size_t parsed_file_count;
    size_t parsed_file_capacity;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Code Generator
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Code generator instance.
 */
struct cg_generator {
    const cg_struct_parser* parser;  /**< Borrowed reference */
    const cg_string_utils* utils;    /**< Borrowed reference */
    cg_expression_handler expr_handler; /**< Expression handler (copied) */
    bool has_expr_handler;
    char* array_wrapper;             /**< Wrapper token for arrays */
    char* optional_wrapper;          /**< Wrapper token for optionals */
    char* oneof_wrapper;             /**< Wrapper token for OneOf */
    char* array_ctor_fmt;            /**< Array constructor format */
    char* optional_empty_fmt;        /**< Optional empty constructor format */
    char* optional_value_prefix_fmt; /**< Optional value prefix format */
    char* optional_value_suffix;     /**< Optional value suffix */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Output
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generated code output.
 */
struct cg_output {
    char* code;               /**< Generated code (owned) */
    size_t length;            /**< Code length */
    size_t capacity;          /**< Buffer capacity */
};

/**
 * @brief Get the directory component of a path.
 * @param path Input path
 * @return Newly allocated directory path or "." if none exists
 */
char* cg_get_directory(const char* path);

/**
 * @brief Join a directory path and file path with '/'.
 * @param dir Directory path
 * @param file File path
 * @return Newly allocated joined path
 */
char* cg_path_join(const char* dir, const char* file);

/**
 * @brief Compute a path to @p target relative to the directory of @p from_path.
 * @param from_path Path whose directory is treated as the base
 * @param target_path Target path to express relative to @p from_path
 * @return Newly allocated relative path
 */
char* cg_path_relative_to_file(const char* from_path, const char* target_path);

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Duplicate a string (NULL-safe).
 * @param s String to duplicate
 * @return Newly allocated copy or NULL
 */
static inline char* cg_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* dup = (char*)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

/**
 * @brief Duplicate a string with length.
 * @param s String to duplicate
 * @param len Number of characters to copy
 * @return Newly allocated copy or NULL
 */
static inline char* cg_strndup(const char* s, size_t len) {
    if (!s) return NULL;
    char* dup = (char*)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}

/**
 * @brief Initialize a cg_error to success state.
 * @param err Error to initialize
 */
static inline void cg_error_init(cg_error* err) {
    if (err) {
        err->code = CG_OK;
        err->message = NULL;
        err->path = NULL;
        err->line = 0;
        err->column = 0;
        err->needs_free = false;
    }
}

/**
 * @brief Set error with static message.
 * @param err Error to set
 * @param code Error code
 * @param message Static message
 */
static inline void cg_error_set(cg_error* err, cg_error_code code, const char* message) {
    if (err) {
        err->code = code;
        err->message = message;
        err->needs_free = false;
    }
}

#endif /* CXGEN_INTERNAL_H */
