/**
 * @file string_utils.c
 * @brief Case conversion utilities for cxgen.
 *
 * Provides snake_case, camelCase, and PascalCase conversions
 * for mapping YAML keys to C++ struct fields.
 */

#include "internal.h"
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Error String Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char* const error_strings[] = {
    [CG_OK] = "Success",
    [CG_ERR_FILE_NOT_FOUND] = "File not found",
    [CG_ERR_PARSE_ERROR] = "Parse error",
    [CG_ERR_TYPE_MISMATCH] = "Type mismatch",
    [CG_ERR_MISSING_FIELD] = "Missing required field",
    [CG_ERR_UNKNOWN_STRUCT] = "Unknown struct type",
    [CG_ERR_UNKNOWN_TYPE] = "Unknown type",
    [CG_ERR_YAML_ERROR] = "YAML parsing error",
    [CG_ERR_OUT_OF_MEMORY] = "Out of memory",
    [CG_ERR_EXPRESSION_ERROR] = "Expression error"
};

const char* cg_error_string(cg_error_code code) {
    if (code >= 0 && code <= CG_ERR_EXPRESSION_ERROR) {
        return error_strings[code];
    }
    return "Unknown error";
}

void cg_error_clear(cg_error* err) {
    if (!err) return;
    if (err->needs_free && err->message) {
        free((void*)err->message);
    }
    cg_error_init(err);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * String Utils Creation/Destruction
 * ═══════════════════════════════════════════════════════════════════════════ */

cg_string_utils* cg_string_utils_new(void) {
    cg_string_utils* utils = (cg_string_utils*)malloc(sizeof(cg_string_utils));
    if (utils) {
        utils->placeholder = 0;
    }
    return utils;
}

void cg_string_utils_free(cg_string_utils* utils) {
    free(utils);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if character is uppercase.
 */
static bool is_upper(char c) {
    return c >= 'A' && c <= 'Z';
}

/**
 * @brief Check if character is lowercase.
 */
static bool is_lower(char c) {
    return c >= 'a' && c <= 'z';
}

/**
 * @brief Check if character is a letter.
 */
static bool is_alpha(char c) {
    return is_upper(c) || is_lower(c);
}

/**
 * @brief Convert character to uppercase.
 */
static char to_upper(char c) {
    if (is_lower(c)) return c - 32;
    return c;
}

/**
 * @brief Convert character to lowercase.
 */
static char to_lower(char c) {
    if (is_upper(c)) return c + 32;
    return c;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Case Conversion Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

char* cg_to_snake_case(const cg_string_utils* utils, const char* s) {
    (void)utils;  /* Currently unused */

    if (!s) return NULL;
    if (!*s) return cg_strdup("");

    /* Calculate required length (worst case: every char needs underscore) */
    size_t len = strlen(s);
    size_t max_len = len * 2 + 1;
    char* result = (char*)malloc(max_len);
    if (!result) return NULL;

    size_t j = 0;
    bool prev_lower = false;
    bool prev_upper = false;

    for (size_t i = 0; s[i]; i++) {
        char c = s[i];

        if (c == '_') {
            /* Keep underscores, reset state */
            result[j++] = '_';
            prev_lower = false;
            prev_upper = false;
        } else if (is_upper(c)) {
            /* Insert underscore before uppercase if:
             * - Previous was lowercase, OR
             * - Previous was uppercase AND next is lowercase (e.g., HTTPServer -> http_server)
             */
            if (prev_lower) {
                result[j++] = '_';
            } else if (prev_upper && s[i+1] && is_lower(s[i+1])) {
                result[j++] = '_';
            }
            result[j++] = to_lower(c);
            prev_lower = false;
            prev_upper = true;
        } else {
            result[j++] = c;
            prev_lower = is_lower(c) || (c >= '0' && c <= '9');
            prev_upper = false;
        }
    }

    result[j] = '\0';
    return result;
}

char* cg_to_camel_case(const cg_string_utils* utils, const char* s) {
    (void)utils;  /* Currently unused */

    if (!s) return NULL;
    if (!*s) return cg_strdup("");

    size_t len = strlen(s);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;

    size_t j = 0;
    bool capitalize_next = false;
    bool first_alpha = true;

    for (size_t i = 0; s[i]; i++) {
        char c = s[i];

        if (c == '_') {
            /* Skip underscore, capitalize next letter */
            capitalize_next = true;
        } else if (is_alpha(c)) {
            if (capitalize_next && !first_alpha) {
                result[j++] = to_upper(c);
            } else if (first_alpha) {
                /* First letter is always lowercase in camelCase */
                result[j++] = to_lower(c);
            } else {
                result[j++] = c;
            }
            capitalize_next = false;
            first_alpha = false;
        } else {
            result[j++] = c;
            capitalize_next = false;
        }
    }

    result[j] = '\0';
    return result;
}

char* cg_to_pascal_case(const cg_string_utils* utils, const char* s) {
    (void)utils;  /* Currently unused */

    if (!s) return NULL;
    if (!*s) return cg_strdup("");

    size_t len = strlen(s);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;

    size_t j = 0;
    bool capitalize_next = true;  /* First char is always uppercase in PascalCase */

    for (size_t i = 0; s[i]; i++) {
        char c = s[i];

        if (c == '_') {
            /* Skip underscore, capitalize next letter */
            capitalize_next = true;
        } else if (is_alpha(c)) {
            if (capitalize_next) {
                result[j++] = to_upper(c);
            } else {
                result[j++] = c;
            }
            capitalize_next = false;
        } else {
            result[j++] = c;
            capitalize_next = false;
        }
    }

    result[j] = '\0';
    return result;
}
