/**
 * @file string_utils.c
 * @brief Case conversion utilities for cxgn.
 *
 * Provides snake_case, camelCase, and PascalCase conversions
 * for mapping YAML keys to C struct fields.
 */

#include "internal.h"
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Error String Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char* const error_strings[] = {
    [CXGN_OK] = "Success",
    [CXGN_ERR_FILE_NOT_FOUND] = "File not found",
    [CXGN_ERR_PARSE_ERROR] = "Parse error",
    [CXGN_ERR_TYPE_MISMATCH] = "Type mismatch",
    [CXGN_ERR_MISSING_FIELD] = "Missing required field",
    [CXGN_ERR_UNKNOWN_STRUCT] = "Unknown struct type",
    [CXGN_ERR_UNKNOWN_TYPE] = "Unknown type",
    [CXGN_ERR_YAML_ERROR] = "YAML parsing error",
    [CXGN_ERR_OUT_OF_MEMORY] = "Out of memory",
    [CXGN_ERR_EXPRESSION_ERROR] = "Expression error",
    [CXGN_ERR_DUPLICATE_KEY] = "Duplicate key",
    [CXGN_ERR_UNKNOWN_FIELD] = "Unknown field",
    [CXGN_ERR_FEATURE_DISABLED] = "Feature disabled",
    [CXGN_ERR_UNSUPPORTED_TYPE] = "Unsupported type"
};

const char* cxgn_error_string(cxgn_error_code code) {
    if (code >= 0 && code <= CXGN_ERR_UNSUPPORTED_TYPE) {
        return error_strings[code];
    }
    return "Unknown error";
}

bool cxgn_has_yaml(void) {
#ifdef CXGN_YAML_SUPPORT
    return true;
#else
    return false;
#endif
}

void cxgn_error_clear(cxgn_error* err) {
    if (!err) return;
    if (err->needs_free && err->message) {
        free((void*)err->message);
    }
    if (err->needs_free && err->path) {
        free((void*)err->path);
    }
    cxgn_error_init(err);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * String Utils Creation/Destruction
 * ═══════════════════════════════════════════════════════════════════════════ */

cxgn_string_utils* cxgn_string_utils_new(void) {
    cxgn_string_utils* utils = (cxgn_string_utils*)malloc(sizeof(cxgn_string_utils));
    if (utils) {
        utils->ref_count = 1;
        utils->placeholder = 0;
    }
    return utils;
}

cxgn_string_utils* cxgn_string_utils_retain(cxgn_string_utils* utils) {
    if (utils) utils->ref_count++;
    return utils;
}

void cxgn_string_utils_free(cxgn_string_utils* utils) {
    if (!utils) return;
    if (utils->ref_count > 1) {
        utils->ref_count--;
        return;
    }
    free(utils);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if character is uppercase.
 */
static bool cxgn_is_upper(char c) {
    return c >= 'A' && c <= 'Z';
}

/**
 * @brief Check if character is lowercase.
 */
static bool cxgn_is_lower(char c) {
    return c >= 'a' && c <= 'z';
}

/**
 * @brief Check if character is a letter.
 */
static bool cxgn_is_alpha(char c) {
    return cxgn_is_upper(c) || cxgn_is_lower(c);
}

/**
 * @brief Convert character to uppercase.
 */
static char cxgn_to_upper_char(char c) {
    if (cxgn_is_lower(c)) return c - 32;
    return c;
}

/**
 * @brief Convert character to lowercase.
 */
static char cxgn_to_lower_char(char c) {
    if (cxgn_is_upper(c)) return c + 32;
    return c;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Case Conversion Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

char* cxgn_to_snake_case(const cxgn_string_utils* utils, const char* s) {
    (void)utils;  /* Currently unused */

    if (!s) return NULL;
    if (!*s) return cxgn_strdup("");

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
        } else if (cxgn_is_upper(c)) {
            /* Insert underscore before uppercase if:
             * - Previous was lowercase, OR
             * - Previous was uppercase AND next is lowercase (e.g., HTTPServer -> http_server)
             */
            if (prev_lower) {
                result[j++] = '_';
            } else if (prev_upper && s[i+1] && cxgn_is_lower(s[i+1])) {
                result[j++] = '_';
            }
            result[j++] = cxgn_to_lower_char(c);
            prev_lower = false;
            prev_upper = true;
        } else {
            result[j++] = c;
            prev_lower = cxgn_is_lower(c) || (c >= '0' && c <= '9');
            prev_upper = false;
        }
    }

    result[j] = '\0';
    return result;
}

char* cxgn_to_camel_case(const cxgn_string_utils* utils, const char* s) {
    (void)utils;  /* Currently unused */

    if (!s) return NULL;
    if (!*s) return cxgn_strdup("");

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
        } else if (cxgn_is_alpha(c)) {
            if (capitalize_next && !first_alpha) {
                result[j++] = cxgn_to_upper_char(c);
            } else if (first_alpha) {
                /* First letter is always lowercase in camelCase */
                result[j++] = cxgn_to_lower_char(c);
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

char* cxgn_to_pascal_case(const cxgn_string_utils* utils, const char* s) {
    (void)utils;  /* Currently unused */

    if (!s) return NULL;
    if (!*s) return cxgn_strdup("");

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
        } else if (cxgn_is_alpha(c)) {
            if (capitalize_next) {
                result[j++] = cxgn_to_upper_char(c);
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
