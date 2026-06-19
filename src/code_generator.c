/**
 * @file code_generator.c
 * @brief Generate pure C11 headers from YAML configuration.
 */

#include "internal.h"
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

typedef enum {
    YAML_VAL_NULL,
    YAML_VAL_BOOL,
    YAML_VAL_INT,
    YAML_VAL_FLOAT,
    YAML_VAL_STRING
} yaml_value_type;

typedef struct {
    cxgn_generator* gen;
    cxgn_output* out;
    cxgn_output* backing;
    cxgn_path* path;
    cxgn_error* err;
    const char* yaml_path;
    const char* header_path;
    int indent;
    int backing_counter;
} gen_context;

static void cxgn_generated_field_cleanup(cxgn_field_info* field) {
    if (!field) return;
    free(field->name);
    free(field->type);
    free(field->default_value);
    free(field->array_elem_type);
    free(field->optional_value_type);
    memset(field, 0, sizeof(*field));
}

static cxgn_output* cxgn_output_new_internal(void) {
    cxgn_output* out = (cxgn_output*)calloc(1, sizeof(*out));
    if (!out) return NULL;
    out->ref_count = 1;
    out->capacity = CXGN_BUFFER_SIZE;
    out->code = (char*)malloc(out->capacity);
    if (!out->code) {
        free(out);
        return NULL;
    }
    out->code[0] = '\0';
    return out;
}

cxgn_output* cxgn_output_retain(cxgn_output* output) {
    if (output) output->ref_count++;
    return output;
}

void cxgn_output_free(cxgn_output* output) {
    if (!output) return;
    if (output->ref_count > 1) {
        output->ref_count--;
        return;
    }
    free(output->code);
    free(output);
}

const char* cxgn_output_get_code(const cxgn_output* output) {
    return output ? output->code : NULL;
}

size_t cxgn_output_get_code_length(const cxgn_output* output) {
    return output ? output->length : 0;
}

static bool cxgn_output_append(cxgn_output* out, const char* str) {
    if (!out || !str) return false;
    size_t len = strlen(str);
    if (out->length + len + 1 > out->capacity) {
        size_t new_cap = out->capacity ? out->capacity * 2 : CXGN_BUFFER_SIZE;
        while (new_cap < out->length + len + 1) new_cap *= 2;
        char* next = (char*)realloc(out->code, new_cap);
        if (!next) return false;
        out->code = next;
        out->capacity = new_cap;
    }
    memcpy(out->code + out->length, str, len + 1);
    out->length += len;
    return true;
}

static bool cxgn_output_appendf(cxgn_output* out, const char* fmt, ...) {
    va_list args;
    va_list args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    const int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) {
        va_end(args_copy);
        return false;
    }

    char* buf = (char*)malloc((size_t)needed + 1);
    if (!buf) {
        va_end(args_copy);
        return false;
    }

    vsnprintf(buf, (size_t)needed + 1, fmt, args_copy);
    va_end(args_copy);

    const bool ok = cxgn_output_append(out, buf);
    free(buf);
    return ok;
}

static const char* cxgn_skip_whitespace_local(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static bool cxgn_starts_with_local(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void cxgn_trim_trailing_whitespace_local(char* s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static void cxgn_gen_indent(gen_context* ctx) {
    for (int i = 0; i < ctx->indent; i++) cxgn_output_append(ctx->out, "    ");
}

static yaml_value_type cxgn_detect_yaml_type(const char* value) {
    if (!value || !*value) return YAML_VAL_NULL;
    if (strcmp(value, "null") == 0 || strcmp(value, "~") == 0) return YAML_VAL_NULL;
    if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) return YAML_VAL_BOOL;

    bool has_digit = false;
    bool has_dot = false;
    bool has_exponent = false;
    const char* p = value;
    if (*p == '-' || *p == '+') p++;
    while (*p) {
        if (*p >= '0' && *p <= '9') has_digit = true;
        else if (*p == '.') {
            if (has_dot || has_exponent) return YAML_VAL_STRING;
            has_dot = true;
        } else if (*p == 'e' || *p == 'E') {
            if (has_exponent || !has_digit) return YAML_VAL_STRING;
            has_exponent = true;
        } else {
            return YAML_VAL_STRING;
        }
        p++;
    }
    if (!has_digit) return YAML_VAL_STRING;
    return has_dot ? YAML_VAL_FLOAT : YAML_VAL_INT;
}

static bool cxgn_yaml_to_bool(const char* value) {
    return strcmp(value, "true") == 0;
}

static bool cxgn_is_unsigned_integer_type(const char* type) {
    if (!type) return false;
    return strcmp(type, "unsigned int") == 0 ||
           strcmp(type, "unsigned long") == 0 ||
           strcmp(type, "unsigned long long") == 0 ||
           strcmp(type, "unsigned short") == 0 ||
           strcmp(type, "unsigned char") == 0 ||
           strcmp(type, "size_t") == 0 ||
           strcmp(type, "uint8_t") == 0 ||
           strcmp(type, "uint16_t") == 0 ||
           strcmp(type, "uint32_t") == 0 ||
           strcmp(type, "uint64_t") == 0;
}

static bool cxgn_has_integer_literal_suffix(const char* value) {
    if (!value || !*value) return false;
    const char* p = value;
    if (*p == '+' || *p == '-') p++;
    while (*p >= '0' && *p <= '9') p++;
    return *p != '\0';
}

static bool cxgn_unsigned_integer_literal_needs_suffix(const char* value) {
    if (!value || !*value) return false;
    if (value[0] == '-') return false;

    /* Only suffix values that cannot be represented by a signed long long.
     * Smaller decimal literals are valid without an unsigned suffix. */
    const unsigned long long limit = (unsigned long long)LLONG_MAX;
    unsigned long long acc = 0;
    const char* p = value;
    if (*p == '+') p++;
    for (; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        const unsigned digit = (unsigned)(*p - '0');
        if (acc > limit / 10ULL) return true;
        acc *= 10ULL;
        if (acc > limit - (unsigned long long)digit) return true;
        acc += (unsigned long long)digit;
    }
    return false;
}

static char* cxgn_escape_string(const char* value) {
    size_t size = 1;
    for (const char* p = value; *p; p++) size += (*p == '"' || *p == '\\' || *p == '\n' || *p == '\t') ? 2 : 1;
    char* out = (char*)malloc(size);
    if (!out) return NULL;
    char* w = out;
    for (const char* p = value; *p; p++) {
        if (*p == '"') { *w++ = '\\'; *w++ = '"'; }
        else if (*p == '\\') { *w++ = '\\'; *w++ = '\\'; }
        else if (*p == '\n') { *w++ = '\\'; *w++ = 'n'; }
        else if (*p == '\t') { *w++ = '\\'; *w++ = 't'; }
        else { *w++ = *p; }
    }
    *w = '\0';
    return out;
}

static char* cxgn_make_backing_name(gen_context* ctx) {
    char* path = cxgn_path_to_string(ctx->path);
    char* name;
    if (!path) return NULL;
    for (char* p = path; *p; p++) {
        if (*p == '.' || *p == '[' || *p == ']') *p = '_';
    }
    const char* pfx = ctx->gen->symbol_prefix ? ctx->gen->symbol_prefix : "";
    const char* suffix = path[0] ? path : "value";
    const int needed = snprintf(NULL, 0, "_%sbacking_%s_%d", pfx, suffix, ctx->backing_counter++);
    if (needed < 0) {
        free(path);
        return NULL;
    }
    name = (char*)malloc((size_t)needed + 1);
    if (!name) {
        free(path);
        return NULL;
    }
    snprintf(name, (size_t)needed + 1, "_%sbacking_%s_%d", pfx, suffix, ctx->backing_counter - 1);
    free(path);
    return name;
}

static char* cxgn_pointer_pointee_type(const char* type) {
    if (!type) return NULL;

    char* copy = cxgn_strdup(type);
    if (!copy) return NULL;
    cxgn_trim_trailing_whitespace_local(copy);

    char* star = strrchr(copy, '*');
    if (!star) {
        free(copy);
        return NULL;
    }
    for (const char* p = star + 1; *p; p++) {
        if (!isspace((unsigned char)*p)) {
            free(copy);
            return NULL;
        }
    }

    *star = '\0';
    cxgn_trim_trailing_whitespace_local(copy);
    char* base = (char*)cxgn_skip_whitespace_local(copy);
    if (cxgn_starts_with_local(base, "const ")) base = (char*)cxgn_skip_whitespace_local(base + strlen("const "));

    if (strcmp(base, "char") == 0) {
        free(copy);
        return NULL;
    }

    char* result = cxgn_strdup(base);
    free(copy);
    return result;
}

static bool cxgn_is_string_literal_type(const char* type) {
    return type &&
           (strcmp(type, "const char*") == 0 ||
            strcmp(type, "std::string_view") == 0 ||
            strcmp(type, "string_view") == 0);
}

static bool cxgn_is_pointer_field_type(const char* type) {
    char* pointee = cxgn_pointer_pointee_type(type);
    const bool is_pointer = pointee != NULL;
    free(pointee);
    return is_pointer;
}

static char* cxgn_unqualified_type_name(const char* type) {
    if (!type) return NULL;

    char* copy = cxgn_strdup(type);
    if (!copy) return NULL;
    cxgn_trim_trailing_whitespace_local(copy);

    char* p = (char*)cxgn_skip_whitespace_local(copy);
    bool advanced = false;
    for (;;) {
        if (cxgn_starts_with_local(p, "const ")) {
            p = (char*)cxgn_skip_whitespace_local(p + strlen("const "));
            advanced = true;
            continue;
        }
        if (cxgn_starts_with_local(p, "volatile ")) {
            p = (char*)cxgn_skip_whitespace_local(p + strlen("volatile "));
            advanced = true;
            continue;
        }
        if (cxgn_starts_with_local(p, "struct ")) {
            p = (char*)cxgn_skip_whitespace_local(p + strlen("struct "));
            advanced = true;
            continue;
        }
        break;
    }

    if (!advanced && p == copy) return copy;

    char* result = cxgn_strdup(p);
    free(copy);
    return result;
}

static const cxgn_type_alias* cxgn_find_alias_relaxed(const cxgn_struct_parser* parser, const char* type) {
    const cxgn_type_alias* alias = cxgn_struct_parser_find_alias(parser, type);
    if (alias) return alias;

    char* unqualified = cxgn_unqualified_type_name(type);
    if (!unqualified) return NULL;
    alias = (strcmp(unqualified, type) == 0)
        ? NULL
        : cxgn_struct_parser_find_alias(parser, unqualified);
    free(unqualified);
    return alias;
}

static const cxgn_struct_info* cxgn_find_struct_relaxed(const cxgn_struct_parser* parser, const char* type) {
    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, type);
    if (info) return info;

    char* unqualified = cxgn_unqualified_type_name(type);
    if (!unqualified) return NULL;
    info = (strcmp(unqualified, type) == 0)
        ? NULL
        : cxgn_struct_parser_find_struct(parser, unqualified);
    free(unqualified);
    return info;
}

static const struct cxgn_enum_info* cxgn_find_enum_relaxed(const cxgn_struct_parser* parser, const char* type) {
    if (!parser || !type) return NULL;
    for (size_t i = 0; i < parser->enum_count; i++) {
        if (strcmp(parser->enums[i].name, type) == 0) return &parser->enums[i];
    }

    char* unqualified = cxgn_unqualified_type_name(type);
    if (!unqualified) return NULL;
    for (size_t i = 0; i < parser->enum_count; i++) {
        if (strcmp(parser->enums[i].name, unqualified) == 0) {
            free(unqualified);
            return &parser->enums[i];
        }
    }
    free(unqualified);
    return NULL;
}

static size_t cxgn_enum_common_prefix_len(const struct cxgn_enum_info* info) {
    if (!info || info->value_count == 0 || !info->values[0].name) return 0;
    size_t prefix_len = strlen(info->values[0].name);
    for (size_t i = 1; i < info->value_count; i++) {
        size_t j = 0;
        const char* name = info->values[i].name;
        while (j < prefix_len && name && info->values[0].name[j] == name[j]) j++;
        prefix_len = j;
    }
    while (prefix_len > 0 && info->values[0].name[prefix_len - 1] != '_') prefix_len--;
    return prefix_len;
}

static bool cxgn_enum_member_matches_yaml(const char* member_name, size_t prefix_len, const char* yaml_value) {
    const char* tail;
    size_t ti = 0;
    size_t yi = 0;
    if (!member_name || !yaml_value) return false;
    if (strcmp(member_name, yaml_value) == 0) return true;
    tail = member_name + prefix_len;
    while (tail[ti] || yaml_value[yi]) {
        while (tail[ti] == '_') ti++;
        while (yaml_value[yi] == '_' || yaml_value[yi] == '-' || yaml_value[yi] == ' ') yi++;
        if (!tail[ti] || !yaml_value[yi]) break;
        if (tolower((unsigned char)tail[ti]) != tolower((unsigned char)yaml_value[yi])) return false;
        ti++;
        yi++;
    }
    while (tail[ti] == '_') ti++;
    while (yaml_value[yi] == '_' || yaml_value[yi] == '-' || yaml_value[yi] == ' ') yi++;
    return tail[ti] == '\0' && yaml_value[yi] == '\0';
}

static const char* cxgn_find_enum_member_for_yaml(const struct cxgn_enum_info* info, const char* yaml_value) {
    if (!info || !yaml_value) return NULL;
    const size_t prefix_len = cxgn_enum_common_prefix_len(info);
    for (size_t i = 0; i < info->value_count; i++) {
        if (cxgn_enum_member_matches_yaml(info->values[i].name, prefix_len, yaml_value)) {
            return info->values[i].name;
        }
    }
    return NULL;
}

static bool cxgn_populate_field_alias_traits(const cxgn_struct_parser* parser, cxgn_field_info* field) {
    const cxgn_type_alias* alias;
    if (!field || !field->type) return true;
    alias = cxgn_find_alias_relaxed(parser, field->type);
    if (!alias) return true;
    if (alias->kind == CXGN_ALIAS_SCALAR) {
        /* Resolve the typedef to its underlying type so cxgn_gen_scalar handles it correctly. */
        char* resolved = cxgn_strdup(alias->value_type);
        if (!resolved) return false;
        free(field->type);
        field->type = resolved;
        /* Re-enter to resolve chained aliases (e.g. typedef expr_t my_t). */
        return cxgn_populate_field_alias_traits(parser, field);
    }
    if (alias->kind == CXGN_ALIAS_ARRAY && !field->is_array) {
        field->is_array = true;
        field->array_elem_type = cxgn_strdup(alias->value_type);
        return field->array_elem_type != NULL;
    }
    if (alias->kind == CXGN_ALIAS_OPTIONAL && !field->is_optional) {
        field->is_optional = true;
        field->optional_value_type = cxgn_strdup(alias->value_type);
        return field->optional_value_type != NULL;
    }
    return true;
}

static bool cxgn_gen_value(gen_context* ctx, const cxgn_node* node, const cxgn_field_info* field);

void cxgn_validation_options_init(cxgn_validation_options* options) {
    if (!options) return;
    options->strict_mode = false;
    options->unknown_field = CXGN_VALIDATION_WARN;
    options->duplicate_key = CXGN_VALIDATION_ERROR;
    options->missing_field = CXGN_VALIDATION_ERROR;
    options->diagnostic_fn = NULL;
    options->diagnostic_userdata = NULL;
}

static bool cxgn_gen_scalar(gen_context* ctx,
                       const cxgn_node* node,
                       const cxgn_field_info* field,
                       const char* source_type) {
    const char* value = NULL;
    char number_buf[64];
    yaml_value_type vtype = YAML_VAL_STRING;
    const char* type = field ? field->type : NULL;
    const char* expr_type = source_type ? source_type : type;

    if (!node) {
        cxgn_error_set(ctx->err, CXGN_ERR_PARSE_ERROR, "Missing scalar node");
        return false;
    }

    switch (node->type) {
        case CXGN_NODE_NULL:
            value = node->raw_scalar_text ? node->raw_scalar_text : "null";
            vtype = YAML_VAL_NULL;
            break;
        case CXGN_NODE_BOOL:
            value = node->raw_scalar_text ? node->raw_scalar_text
                                          : (node->as.bool_value ? "true" : "false");
            vtype = YAML_VAL_BOOL;
            break;
        case CXGN_NODE_INTEGER:
            if (node->raw_scalar_text) {
                value = node->raw_scalar_text;
            } else {
                if (snprintf(number_buf, sizeof(number_buf), "%lld", node->as.int_value) < 0) {
                    cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Failed formatting integer scalar");
                    return false;
                }
                value = number_buf;
            }
            vtype = YAML_VAL_INT;
            break;
        case CXGN_NODE_FLOAT:
            if (node->raw_scalar_text) {
                value = node->raw_scalar_text;
            } else {
                if (snprintf(number_buf, sizeof(number_buf), "%.17g", node->as.float_value) < 0) {
                    cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Failed formatting float scalar");
                    return false;
                }
                value = number_buf;
            }
            vtype = YAML_VAL_FLOAT;
            break;
        case CXGN_NODE_STRING:
            value = node->as.string.data ? node->as.string.data : "";
            vtype = cxgn_detect_yaml_type(value);
            break;
        default:
            cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Expected scalar node");
            return false;
    }

    if (ctx->gen->has_expr_handler && expr_type &&
        ctx->gen->expr_handler.is_expression_field &&
        ctx->gen->expr_handler.is_expression_field(expr_type, ctx->gen->expr_handler.userdata)) {
        char* yaml_path = cxgn_path_to_string(ctx->path);
        char* validation = ctx->gen->expr_handler.validate
            ? ctx->gen->expr_handler.validate(value, yaml_path, ctx->gen->expr_handler.userdata)
            : NULL;
        if (validation) {
            if (ctx->err) {
                ctx->err->code = CXGN_ERR_EXPRESSION_ERROR;
                ctx->err->message = validation;
                ctx->err->path = yaml_path;
                ctx->err->needs_free = true;
                ctx->err->line = 0;
                ctx->err->column = 0;
            } else {
                free(validation);
            }
            return false;
        }

        char* code = ctx->gen->expr_handler.generate_code
            ? ctx->gen->expr_handler.generate_code(value, yaml_path, ctx->gen->expr_handler.userdata)
            : NULL;
        if (!code) {
            free(yaml_path);
            cxgn_error_set(ctx->err, CXGN_ERR_EXPRESSION_ERROR, "Expression generation failed");
            return false;
        }
        cxgn_output_append(ctx->out, code);
        free(code);
        free(yaml_path);
        return true;
    }

    const struct cxgn_enum_info* enum_info = type ? cxgn_find_enum_relaxed(ctx->gen->parser, type) : NULL;
    if (enum_info) {
        if (node->type == CXGN_NODE_STRING) {
            const char* enum_member = cxgn_find_enum_member_for_yaml(enum_info, value);
            if (!enum_member) {
                char detail[512];
                snprintf(detail, sizeof(detail), "Unknown enum value '%s' for type %s", value, type);
                cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, detail);
                return false;
            }
            return cxgn_output_append(ctx->out, enum_member);
        }
        if (vtype == YAML_VAL_INT) {
            return cxgn_output_append(ctx->out, value);
        }
        cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Expected enum string or integer");
        return false;
    }

    if (type && strcmp(type, "bool") == 0) {
        return cxgn_output_append(ctx->out, cxgn_yaml_to_bool(value) ? "true" : "false");
    }
    if (type && cxgn_is_unsigned_integer_type(type) && vtype == YAML_VAL_INT) {
        cxgn_output_append(ctx->out, value);
        if (!cxgn_has_integer_literal_suffix(value) && cxgn_unsigned_integer_literal_needs_suffix(value)) {
            cxgn_output_append(ctx->out, "ULL");
        }
        return true;
    }
    if (type && (strcmp(type, "float") == 0 || strcmp(type, "double") == 0 || strcmp(type, "long double") == 0)) {
        cxgn_output_append(ctx->out, value);
        if (vtype == YAML_VAL_INT) cxgn_output_append(ctx->out, ".0");
        return true;
    }
    if (cxgn_is_string_literal_type(type)) {
        if (node->type == CXGN_NODE_NULL) return cxgn_output_append(ctx->out, "0");
        char* escaped = cxgn_escape_string(value);
        if (!escaped) return false;
        const bool ok = cxgn_output_appendf(ctx->out, "\"%s\"", escaped);
        free(escaped);
        return ok;
    }

    if (vtype == YAML_VAL_STRING) {
        char* escaped = cxgn_escape_string(value);
        if (!escaped) return false;
        const bool ok = cxgn_output_appendf(ctx->out, "\"%s\"", escaped);
        free(escaped);
        return ok;
    }
    if (vtype == YAML_VAL_BOOL) return cxgn_output_append(ctx->out, cxgn_yaml_to_bool(value) ? "true" : "false");
    return cxgn_output_append(ctx->out, value);
}

static char* cxgn_vasprintf(const char* fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    const int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) return NULL;

    char* buf = (char*)malloc((size_t)needed + 1);
    if (!buf) return NULL;
    vsnprintf(buf, (size_t)needed + 1, fmt, args);
    return buf;
}

static char* cxgn_asprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* buf = cxgn_vasprintf(fmt, args);
    va_end(args);
    return buf;
}

static char* cxgn_make_child_path(const cxgn_path* path, const char* child) {
    char* base = cxgn_path_to_string(path);
    char* full;
    if (!base) return NULL;
    if (!child || !child[0]) return base;
    if (!base[0]) {
        free(base);
        return cxgn_strdup(child);
    }
    full = cxgn_asprintf("%s.%s", base, child);
    free(base);
    return full;
}

static void cxgn_apply_node_mark(cxgn_error* err, const cxgn_node* node) {
    if (!err || !node) return;
    err->line = node->line;
    err->column = node->column;
}

static bool cxgn_emit_validation(gen_context* ctx,
                            cxgn_validation_action action,
                            cxgn_error_code code,
                            const cxgn_node* node,
                            size_t line,
                            size_t column,
                            const char* path,
                            const char* fmt, ...) {
    if (ctx->gen->validation.strict_mode && action != CXGN_VALIDATION_IGNORE) {
        action = CXGN_VALIDATION_ERROR;
    }
    if (action == CXGN_VALIDATION_IGNORE) return true;

    va_list args;
    va_start(args, fmt);
    char* message = cxgn_vasprintf(fmt, args);
    va_end(args);
    if (!message) {
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    char* owned_path = path ? cxgn_strdup(path) : NULL;
    if (path && !owned_path) {
        free(message);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    cxgn_error diagnostic = {0};
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.path = owned_path;
    diagnostic.needs_free = true;
    cxgn_apply_node_mark(&diagnostic, node);
    if (diagnostic.line == 0) diagnostic.line = line;
    if (diagnostic.column == 0) diagnostic.column = column;

    if (ctx->gen->validation.diagnostic_fn) {
        cxgn_diagnostic_level level = action == CXGN_VALIDATION_ERROR
            ? CXGN_DIAGNOSTIC_ERROR
            : CXGN_DIAGNOSTIC_WARNING;
        ctx->gen->validation.diagnostic_fn(level, &diagnostic,
                                           ctx->gen->validation.diagnostic_userdata);
    }

    if (action == CXGN_VALIDATION_ERROR) {
        if (ctx->err) {
            cxgn_error_clear(ctx->err);
            *ctx->err = diagnostic;
        } else {
            cxgn_error_clear(&diagnostic);
        }
        return false;
    }

    cxgn_error_clear(&diagnostic);
    return true;
}

static size_t cxgn_find_field_index(const gen_context* ctx,
                               const cxgn_struct_info* info,
                               const char* key_text) {
    for (size_t i = 0; i < info->field_count; i++) {
        const cxgn_field_info* field = &info->fields[i];
        if (strcmp(key_text, field->name) == 0) return i;

        char* snake = cxgn_to_snake_case(ctx->gen->utils, field->name);
        const bool match = snake && strcmp(key_text, snake) == 0;
        free(snake);
        if (match) return i;
    }
    return (size_t)-1;
}

static const cxgn_node* cxgn_find_field_value_node(const gen_context* ctx,
                                              const cxgn_node* object_node,
                                              const cxgn_field_info* field) {
    char* snake;
    const cxgn_node* value = NULL;

    if (!object_node || object_node->type != CXGN_NODE_OBJECT || !field) return NULL;

    snake = cxgn_to_snake_case(ctx->gen->utils, field->name);
    for (size_t i = 0; i < object_node->as.object.count; i++) {
        const cxgn_object_entry* entry = &object_node->as.object.entries[i];
        if (strcmp(entry->key, field->name) == 0 || (snake && strcmp(entry->key, snake) == 0)) {
            value = entry->value;
            break;
        }
    }
    free(snake);
    return value;
}

static bool cxgn_validate_struct_mapping(gen_context* ctx,
                                    const cxgn_node* node,
                                    const cxgn_struct_info* info) {
    bool* seen = NULL;
    bool had_error = false;
    if (info->field_count > 0) {
        seen = (bool*)calloc(info->field_count, sizeof(*seen));
        if (!seen) {
            cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }
    }

    for (size_t i = 0; i < node->as.object.count; i++) {
        const cxgn_object_entry* entry = &node->as.object.entries[i];
        const char* key_text = entry->key;
        size_t field_index = cxgn_find_field_index(ctx, info, key_text);
        char* key_path = cxgn_make_child_path(ctx->path, key_text);
        if (!key_path) {
            free(seen);
            cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }

        if (field_index == (size_t)-1) {
            const bool ok = cxgn_emit_validation(ctx,
                                            ctx->gen->validation.unknown_field,
                                            CXGN_ERR_UNKNOWN_FIELD,
                                            entry->value,
                                            entry->line,
                                            entry->column,
                                            key_path,
                                            "Unknown field '%s'",
                                            key_text);
            free(key_path);
            if (!ok) {
                if (ctx->err && ctx->err->code == CXGN_ERR_OUT_OF_MEMORY) {
                    free(seen);
                    return false;
                }
                had_error = true;
            }
            continue;
        }

        if (seen[field_index]) {
            const bool ok = cxgn_emit_validation(ctx,
                                            ctx->gen->validation.duplicate_key,
                                            CXGN_ERR_DUPLICATE_KEY,
                                            entry->value,
                                            entry->line,
                                            entry->column,
                                            key_path,
                                            "Duplicate mapping for field '%s'",
                                            info->fields[field_index].name);
            free(key_path);
            if (!ok) {
                if (ctx->err && ctx->err->code == CXGN_ERR_OUT_OF_MEMORY) {
                    free(seen);
                    return false;
                }
                had_error = true;
            }
            continue;
        }

        seen[field_index] = true;
        free(key_path);
    }

    for (size_t i = 0; i < info->field_count; i++) {
        if (seen && seen[i]) continue;
        if (info->fields[i].is_optional) continue;
        if (cxgn_is_pointer_field_type(info->fields[i].type)) continue;
        if (cxgn_is_string_literal_type(info->fields[i].type)) continue;

        char* field_path = cxgn_make_child_path(ctx->path, info->fields[i].name);
        if (!field_path) {
            free(seen);
            cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }

        const bool ok = cxgn_emit_validation(ctx,
                                        ctx->gen->validation.missing_field,
                                        CXGN_ERR_MISSING_FIELD,
                                        node,
                                        node->line,
                                        node->column,
                                        field_path,
                                        "Missing required field '%s'",
                                        info->fields[i].name);
        free(field_path);
        if (!ok) {
            if (ctx->err && ctx->err->code == CXGN_ERR_OUT_OF_MEMORY) {
                free(seen);
                return false;
            }
            had_error = true;
        }
    }

    free(seen);
    return !had_error;
}

static const cxgn_object_entry* cxgn_find_object_entry_exact(const cxgn_node* object_node,
                                                        const char* key) {
    if (!object_node || object_node->type != CXGN_NODE_OBJECT || !key) return NULL;
    for (size_t i = 0; i < object_node->as.object.count; i++) {
        const cxgn_object_entry* entry = &object_node->as.object.entries[i];
        if (strcmp(entry->key, key) == 0) return entry;
    }
    return NULL;
}

static void cxgn_set_context_error(gen_context* ctx,
                              cxgn_error_code code,
                              const cxgn_node* node,
                              const char* message) {
    if (!ctx || !ctx->err) return;
    char* path = cxgn_path_to_string(ctx->path);
    char* owned_message = cxgn_strdup(message ? message : cxgn_error_string(code));
    if (!owned_message) {
        free(path);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return;
    }
    cxgn_error_clear(ctx->err);
    ctx->err->code = code;
    ctx->err->message = owned_message;
    ctx->err->path = path;
    ctx->err->line = node ? node->line : 0;
    ctx->err->column = node ? node->column : 0;
    ctx->err->needs_free = true;
}

static bool cxgn_gen_struct_value(gen_context* ctx, const cxgn_node* node,
                             const cxgn_struct_info* info) {
    if (!node || node->type != CXGN_NODE_OBJECT) {
        cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Expected mapping for struct");
        return false;
    }

    if (!cxgn_validate_struct_mapping(ctx, node, info)) {
        return false;
    }

    cxgn_output_append(ctx->out, "{\n");
    ctx->indent++;

    for (size_t i = 0; i < info->field_count; i++) {
        const cxgn_field_info* field = &info->fields[i];
        const cxgn_node* value_node = cxgn_find_field_value_node(ctx, node, field);

        cxgn_gen_indent(ctx);
        cxgn_output_appendf(ctx->out, ".%s = ", field->name);
        if (value_node) {
            cxgn_path_push(ctx->path, field->name);
            if (!cxgn_gen_value(ctx, value_node, field)) {
                cxgn_path_pop(ctx->path);
                return false;
            }
            cxgn_path_pop(ctx->path);
        } else if (field->is_optional) {
            cxgn_output_append(ctx->out, "{.has_value = false}");
        } else if (cxgn_is_pointer_field_type(field->type) || cxgn_is_string_literal_type(field->type)) {
            cxgn_output_append(ctx->out, "0");
        } else {
            cxgn_output_append(ctx->out, "{0}");
        }

        if (i + 1 < info->field_count) cxgn_output_append(ctx->out, ",");
        cxgn_output_append(ctx->out, "\n");
    }

    ctx->indent--;
    cxgn_gen_indent(ctx);
    cxgn_output_append(ctx->out, "}");
    return true;
}

static bool cxgn_gen_array(gen_context* ctx, const cxgn_node* node,
                      const cxgn_field_info* field) {
    if (!node || node->type != CXGN_NODE_ARRAY) {
        cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Expected sequence for array");
        return false;
    }

    char* backing_name = cxgn_make_backing_name(ctx);
    cxgn_output* temp = cxgn_output_new_internal();
    if (!backing_name || !temp) {
        free(backing_name);
        cxgn_output_free(temp);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    size_t count = 0;
    for (size_t i = 0; i < node->as.array.count; i++, count++) {
        const cxgn_node* elem = node->as.array.items[i];
        if (count > 0) cxgn_output_append(temp, ", ");
        cxgn_field_info elem_field = {0};
        elem_field.type = cxgn_strdup(field->array_elem_type);
        if (!elem_field.type || !cxgn_populate_field_alias_traits(ctx->gen->parser, &elem_field)) {
            cxgn_generated_field_cleanup(&elem_field);
            cxgn_output_free(temp);
            free(backing_name);
            cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }
        cxgn_output* saved = ctx->out;
        ctx->out = temp;
        cxgn_path_push_index(ctx->path, count);
        const bool ok = cxgn_gen_value(ctx, elem, &elem_field);
        cxgn_path_pop(ctx->path);
        ctx->out = saved;
        cxgn_generated_field_cleanup(&elem_field);
        if (!ok) {
            cxgn_output_free(temp);
            free(backing_name);
            return false;
        }
    }

    const char* storage_elem_type = field->array_elem_type;
    if (!storage_elem_type) {
        cxgn_output_free(temp);
        free(backing_name);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }
    if (cxgn_starts_with_local(storage_elem_type, "const ") && strchr(storage_elem_type, '*') == NULL) {
        cxgn_output_appendf(ctx->backing, "static %s const %s_data[] = {%s};\n",
                            storage_elem_type + strlen("const "), backing_name, temp->code);
    } else {
        cxgn_output_appendf(ctx->backing, "static %s const %s_data[] = {%s};\n",
                            storage_elem_type, backing_name, temp->code);
    }
    cxgn_output_appendf(ctx->out, "{.data = %s_data, .count = %zu}", backing_name, count);

    cxgn_output_free(temp);
    free(backing_name);
    return true;
}

static bool cxgn_gen_optional(gen_context* ctx, const cxgn_node* node,
                         const cxgn_field_info* field) {
    if (node && node->type == CXGN_NODE_NULL) {
        return cxgn_output_append(ctx->out, "{.has_value = false}");
    }

    cxgn_output_append(ctx->out, "{.value = ");
    cxgn_field_info inner = {0};
    inner.type = cxgn_strdup(field->optional_value_type);
    if (!inner.type || !cxgn_populate_field_alias_traits(ctx->gen->parser, &inner)) {
        cxgn_generated_field_cleanup(&inner);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }
    const bool ok = cxgn_gen_value(ctx, node, &inner);
    cxgn_generated_field_cleanup(&inner);
    cxgn_output_append(ctx->out, ", .has_value = true}");
    return ok;
}

static bool cxgn_gen_pointer(gen_context* ctx, const cxgn_node* node,
                        const cxgn_field_info* field) {
    if (!node || node->type == CXGN_NODE_NULL) {
        return cxgn_output_append(ctx->out, "0");
    }

    char* pointee_type = cxgn_pointer_pointee_type(field->type);
    char* backing_name = cxgn_make_backing_name(ctx);
    cxgn_output* temp = cxgn_output_new_internal();
    if (!pointee_type || !backing_name || !temp) {
        free(pointee_type);
        free(backing_name);
        cxgn_output_free(temp);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    cxgn_field_info pointee = {0};
    pointee.type = cxgn_strdup(pointee_type);
    if (!pointee.type || !cxgn_populate_field_alias_traits(ctx->gen->parser, &pointee)) {
        cxgn_generated_field_cleanup(&pointee);
        free(pointee_type);
        free(backing_name);
        cxgn_output_free(temp);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    cxgn_output* saved = ctx->out;
    ctx->out = temp;
    const bool ok = cxgn_gen_value(ctx, node, &pointee);
    ctx->out = saved;
    cxgn_generated_field_cleanup(&pointee);
    if (!ok) {
        free(pointee_type);
        free(backing_name);
        cxgn_output_free(temp);
        return false;
    }

    cxgn_output_appendf(ctx->backing, "static const %s %s = %s;\n",
                   pointee_type, backing_name, temp->code);
    cxgn_output_appendf(ctx->out, "&%s", backing_name);

    free(pointee_type);
    free(backing_name);
    cxgn_output_free(temp);
    return true;
}

static bool cxgn_gen_value(gen_context* ctx, const cxgn_node* node,
                      const cxgn_field_info* field) {
    cxgn_field_info derived = {0};
    derived.type = cxgn_strdup(field->type);
    derived.is_array = field->is_array;
    derived.is_optional = field->is_optional;
    derived.array_elem_type = field->array_elem_type ? cxgn_strdup(field->array_elem_type) : NULL;
    derived.optional_value_type = field->optional_value_type ? cxgn_strdup(field->optional_value_type) : NULL;
    if (!derived.type || (field->array_elem_type && !derived.array_elem_type) ||
        (field->optional_value_type && !derived.optional_value_type)) {
        cxgn_generated_field_cleanup(&derived);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }
    if (!cxgn_populate_field_alias_traits(ctx->gen->parser, &derived)) {
        cxgn_generated_field_cleanup(&derived);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    if (derived.is_array) {
        const bool ok = cxgn_gen_array(ctx, node, &derived);
        cxgn_generated_field_cleanup(&derived);
        return ok;
    }
    if (derived.is_optional) {
        const bool ok = cxgn_gen_optional(ctx, node, &derived);
        cxgn_generated_field_cleanup(&derived);
        return ok;
    }
    if (cxgn_is_pointer_field_type(derived.type)) {
        const bool ok = cxgn_gen_pointer(ctx, node, &derived);
        cxgn_generated_field_cleanup(&derived);
        return ok;
    }
    if (node->type == CXGN_NODE_NULL || node->type == CXGN_NODE_BOOL ||
        node->type == CXGN_NODE_INTEGER || node->type == CXGN_NODE_FLOAT ||
        node->type == CXGN_NODE_STRING) {
        const bool ok = cxgn_gen_scalar(ctx, node, &derived, field->type);
        cxgn_generated_field_cleanup(&derived);
        return ok;
    }
    if (node->type == CXGN_NODE_OBJECT) {
        const cxgn_struct_info* nested = cxgn_find_struct_relaxed(ctx->gen->parser, derived.type);
        if (!nested) {
            char detail[512];
            snprintf(detail, sizeof(detail), "Unknown struct type: %s",
                     derived.type ? derived.type : "<null>");
            cxgn_generated_field_cleanup(&derived);
            cxgn_error_set(ctx->err, CXGN_ERR_UNKNOWN_STRUCT, detail);
            return false;
        }
        const bool ok = cxgn_gen_struct_value(ctx, node, nested);
        cxgn_generated_field_cleanup(&derived);
        return ok;
    }

    cxgn_generated_field_cleanup(&derived);
    cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Unexpected YAML node type");
    return false;
}

cxgn_generator* cxgn_generator_retain(cxgn_generator* gen) {
    if (gen) gen->ref_count++;
    return gen;
}

cxgn_generator* cxgn_generator_new(const cxgn_struct_parser* parser, const cxgn_string_utils* utils) {
    if (!parser) return NULL;
    cxgn_string_utils* retained_utils =
        cxgn_string_utils_retain((cxgn_string_utils*)(utils ? utils : parser->utils));
    if (!retained_utils) return NULL;

    cxgn_generator* gen = (cxgn_generator*)calloc(1, sizeof(*gen));
    if (!gen) {
        cxgn_string_utils_free(retained_utils);
        return NULL;
    }
    gen->ref_count = 1;
    gen->parser = cxgn_struct_parser_retain((cxgn_struct_parser*)parser);
    gen->utils = retained_utils;
    gen->cpp_std = CXGN_CPP_STD_20;
    cxgn_validation_options_init(&gen->validation);
    return gen;
}

void cxgn_generator_free(cxgn_generator* gen) {
    if (!gen) return;
    if (gen->ref_count > 1) {
        gen->ref_count--;
        return;
    }
    free(gen->helpers_header);
    free(gen->root_struct_name);
    free(gen->symbol_prefix);
    cxgn_struct_parser_free(gen->parser);
    cxgn_string_utils_free(gen->utils);
    free(gen);
}

void cxgn_generator_set_validation_options(cxgn_generator* gen,
                                           const cxgn_validation_options* options) {
    if (!gen || !options) return;
    gen->validation = *options;
}

void cxgn_generator_set_strict_mode(cxgn_generator* gen, bool strict) {
    if (!gen) return;
    gen->validation.strict_mode = strict;
}

void cxgn_generator_set_cpp_std(cxgn_generator* gen, cxgn_cpp_std std) {
    if (gen) gen->cpp_std = std;
}

void cxgn_generator_set_helpers_header(cxgn_generator* gen, const char* helpers_header) {
    if (!gen) return;
    char* next = helpers_header ? cxgn_strdup(helpers_header) : NULL;
    if (helpers_header && !next) return;
    free(gen->helpers_header);
    gen->helpers_header = next;
}

void cxgn_generator_set_root_struct(cxgn_generator* gen, const char* root_struct_name) {
    if (!gen) return;
    char* next = root_struct_name ? cxgn_strdup(root_struct_name) : NULL;
    if (root_struct_name && !next) return;
    free(gen->root_struct_name);
    gen->root_struct_name = next;
}

void cxgn_generator_set_symbol_prefix(cxgn_generator* gen, const char* prefix) {
    if (!gen) return;
    char* next = prefix ? cxgn_strdup(prefix) : NULL;
    if (prefix && !next) return;
    free(gen->symbol_prefix);
    gen->symbol_prefix = next;
}

void cxgn_generator_set_expression_handler(cxgn_generator* gen, const cxgn_expression_handler* handler) {
    if (!gen || !handler) return;
    gen->expr_handler = *handler;
    gen->has_expr_handler = true;
}

void cxgn_generator_set_type_options(cxgn_generator* gen, const cxgn_type_options* options) {
    (void)gen;
    (void)options;
}

static bool cxgn_emit_helper_typedefs(cxgn_output* out, const cxgn_generator* gen) {
    if (gen->helpers_header) return cxgn_output_appendf(out, "#include <%s>\n\n", gen->helpers_header);
    return true;
}

static void cxgn_set_located_error(cxgn_error* err,
                              cxgn_error_code code,
                              const char* message,
                              const char* path,
                              size_t line,
                              size_t column) {
    char* owned_message = NULL;
    char* owned_path = NULL;

    if (!err) return;

    owned_message = cxgn_strdup(message ? message : cxgn_error_string(code));
    if (!owned_message) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return;
    }

    if (path) {
        owned_path = cxgn_strdup(path);
        if (!owned_path) {
            free(owned_message);
            cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return;
        }
    }

    cxgn_error_clear(err);
    err->code = code;
    err->message = owned_message;
    err->path = owned_path;
    err->line = line;
    err->column = column;
    err->needs_free = true;
}

cxgn_output* cxgn_generate_from_document(cxgn_generator* gen, const cxgn_document* doc,
                                         const char* yaml_path, const char* header_path,
                                         cxgn_error* err) {
    const cxgn_node* root = doc ? doc->root : NULL;
    if (!root || root->type != CXGN_NODE_OBJECT) {
        cxgn_set_located_error(err, CXGN_ERR_YAML_ERROR, "YAML root must be a mapping",
                          yaml_path, root ? root->line : 0, root ? root->column : 0);
        return NULL;
    }

    const size_t count = cxgn_struct_parser_get_struct_count(gen->parser);
    const cxgn_struct_info* root_struct = NULL;
    if (gen->root_struct_name && gen->root_struct_name[0]) {
        root_struct = cxgn_struct_parser_find_struct(gen->parser, gen->root_struct_name);
        if (!root_struct) {
            char detail[512];
            snprintf(detail, sizeof(detail), "Root struct '%s' not found in header",
                     gen->root_struct_name);
            cxgn_error_set(err, CXGN_ERR_UNKNOWN_STRUCT, detail);
            return NULL;
        }
    } else if (count) {
        root_struct = cxgn_struct_parser_get_struct(gen->parser, count - 1);
    }
    if (!root_struct) {
        cxgn_error_set(err, CXGN_ERR_UNKNOWN_STRUCT, "No struct found in header");
        return NULL;
    }

    cxgn_output* out = cxgn_output_new_internal();
    cxgn_output* backing = cxgn_output_new_internal();
    cxgn_path* path = cxgn_path_new();
    if (!out || !backing || !path) {
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_path_free(path);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    gen_context ctx = {
        .gen = gen,
        .out = out,
        .backing = backing,
        .path = path,
        .err = err,
        .yaml_path = yaml_path,
        .header_path = header_path,
    };

    const char* sym_pfx = gen->symbol_prefix ? gen->symbol_prefix : "";
    cxgn_path_push(path, root_struct->name);
    cxgn_output_appendf(out, "static const %s %sconfig = ", root_struct->name, sym_pfx);
    if (!cxgn_gen_struct_value(&ctx, root, root_struct)) {
        cxgn_path_free(path);
        cxgn_output_free(out);
        cxgn_output_free(backing);
        return NULL;
    }
    cxgn_path_pop(path);
    cxgn_output_append(out, ";\n");

    cxgn_output* final = cxgn_output_new_internal();
    if (!final) {
        cxgn_path_free(path);
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    if (!cxgn_emit_helper_typedefs(final, gen)) {
        cxgn_path_free(path);
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_output_free(final);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }
    cxgn_output_append(final, backing->code);
    if (backing->length > 0) cxgn_output_append(final, "\n");
    cxgn_output_append(final, out->code);

    cxgn_path_free(path);
    cxgn_output_free(out);
    cxgn_output_free(backing);
    return final;
}

cxgn_output* cxgn_generate(cxgn_generator* gen, const char* yaml_path,
                           const char* header_path, cxgn_error* err) {
    cxgn_error_init(err);
    if (!gen || !yaml_path || !header_path) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return NULL;
    }

    cxgn_document* doc = cxgn_document_from_yaml_file(yaml_path, err);
    if (!doc) return NULL;

    cxgn_output* result = cxgn_generate_from_document(gen, doc, yaml_path, header_path, err);
    cxgn_document_free(doc);
    return result;
}

cxgn_output* cxgn_generate_from_yaml_text(cxgn_generator* gen, const char* yaml_text,
                                          const char* yaml_virtual_path, const char* header_path,
                                          cxgn_error* err) {
    cxgn_error_init(err);
    if (!gen || !yaml_text || !header_path) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return NULL;
    }

    const char* diag_path = (yaml_virtual_path && yaml_virtual_path[0]) ? yaml_virtual_path : "<in-memory-yaml>";
    cxgn_document* doc = cxgn_document_from_yaml_text(yaml_text, diag_path, err);
    if (!doc) return NULL;

    cxgn_output* result = cxgn_generate_from_document(gen, doc, diag_path, header_path, err);
    cxgn_document_free(doc);
    return result;
}
