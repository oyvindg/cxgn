/**
 * @file code_generator.c
 * @brief Generate constexpr C++ code from YAML configuration.
 *
 * Maps YAML values to C++ struct fields, validates types,
 * and generates constexpr initialization code.
 */

#include "internal.h"
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <yaml.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Output Buffer
 * ═══════════════════════════════════════════════════════════════════════════ */

static cg_output* output_new(void) {
    cg_output* out = (cg_output*)calloc(1, sizeof(cg_output));
    if (!out) return NULL;

    out->capacity = CG_BUFFER_SIZE;
    out->code = (char*)malloc(out->capacity);
    if (!out->code) {
        free(out);
        return NULL;
    }
    out->code[0] = '\0';
    out->length = 0;
    return out;
}

static bool output_append(cg_output* out, const char* str) {
    if (!out || !str) return false;

    size_t len = strlen(str);
    if (out->length + len + 1 > out->capacity) {
        size_t new_cap = out->capacity * 2;
        while (new_cap < out->length + len + 1) {
            new_cap *= 2;
        }
        char* new_code = (char*)realloc(out->code, new_cap);
        if (!new_code) return false;
        out->code = new_code;
        out->capacity = new_cap;
    }

    memcpy(out->code + out->length, str, len + 1);
    out->length += len;
    return true;
}

static bool output_appendf(cg_output* out, const char* fmt, ...) {
    char buf[CG_LINE_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return output_append(out, buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * YAML Value Type Detection
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    YAML_VAL_NULL,
    YAML_VAL_BOOL,
    YAML_VAL_INT,
    YAML_VAL_FLOAT,
    YAML_VAL_STRING
} yaml_value_type;

static yaml_value_type detect_yaml_type(const char* value) {
    if (!value || !*value) return YAML_VAL_NULL;

    /* Check for null */
    if (strcmp(value, "null") == 0 || strcmp(value, "~") == 0 ||
        strcmp(value, "Null") == 0 || strcmp(value, "NULL") == 0) {
        return YAML_VAL_NULL;
    }

    /* Check for boolean */
    if (strcmp(value, "true") == 0 || strcmp(value, "True") == 0 ||
        strcmp(value, "TRUE") == 0 || strcmp(value, "yes") == 0 ||
        strcmp(value, "Yes") == 0 || strcmp(value, "YES") == 0 ||
        strcmp(value, "on") == 0 || strcmp(value, "On") == 0 ||
        strcmp(value, "ON") == 0) {
        return YAML_VAL_BOOL;
    }
    if (strcmp(value, "false") == 0 || strcmp(value, "False") == 0 ||
        strcmp(value, "FALSE") == 0 || strcmp(value, "no") == 0 ||
        strcmp(value, "No") == 0 || strcmp(value, "NO") == 0 ||
        strcmp(value, "off") == 0 || strcmp(value, "Off") == 0 ||
        strcmp(value, "OFF") == 0) {
        return YAML_VAL_BOOL;
    }

    /* Check for number */
    const char* p = value;
    bool has_dot = false;
    bool has_digit = false;

    if (*p == '-' || *p == '+') p++;

    while (*p) {
        if (*p == '.') {
            if (has_dot) return YAML_VAL_STRING;
            has_dot = true;
        } else if (*p >= '0' && *p <= '9') {
            has_digit = true;
        } else if (*p == 'e' || *p == 'E') {
            p++;
            if (*p == '-' || *p == '+') p++;
            continue;
        } else {
            return YAML_VAL_STRING;
        }
        p++;
    }

    if (!has_digit) return YAML_VAL_STRING;
    return has_dot ? YAML_VAL_FLOAT : YAML_VAL_INT;
}

static bool yaml_to_bool(const char* value) {
    return strcmp(value, "true") == 0 || strcmp(value, "True") == 0 ||
           strcmp(value, "TRUE") == 0 || strcmp(value, "yes") == 0 ||
           strcmp(value, "Yes") == 0 || strcmp(value, "YES") == 0 ||
           strcmp(value, "on") == 0 || strcmp(value, "On") == 0 ||
           strcmp(value, "ON") == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Code Generation Context
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    cg_generator* gen;
    cg_output* out;
    cg_output* backing;  /* Backing storage for arrays */
    cg_path* path;
    cg_error* err;
    const char* yaml_path;
    const char* header_path;
    int indent;
    int backing_counter;
} gen_context;

static void warn_yellowf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\033[1;33mWarning:\033[0m ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static void warn_redf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\033[1;31mWarning:\033[0m ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static void warn_missing_header_field(gen_context* ctx, const cg_struct_info* info,
                                      const cg_field_info* field, const char* reason) {
    char* base_path = cg_path_to_string(ctx->path);
    if (!base_path) return;

    if (base_path[0] != '\0') {
        warn_redf("Header field '%s.%s' is missing in YAML (%s). %s [yaml: %s:1, header: %s:1]",
                  base_path, field->name, info->name, reason,
                  ctx->yaml_path ? ctx->yaml_path : "<unknown>",
                  ctx->header_path ? ctx->header_path : "<unknown>");
    } else {
        warn_redf("Header field '%s' is missing in YAML (%s). %s [yaml: %s:1, header: %s:1]",
                  field->name, info->name, reason,
                  ctx->yaml_path ? ctx->yaml_path : "<unknown>",
                  ctx->header_path ? ctx->header_path : "<unknown>");
    }

    free(base_path);
}

static void warn_unknown_yaml_key(gen_context* ctx, const cg_struct_info* info,
                                  const char* yaml_key) {
    char* base_path = cg_path_to_string(ctx->path);
    if (!base_path) return;

    if (base_path[0] != '\0') {
        warn_yellowf("YAML field '%s.%s' is not defined in header struct '%s' and will be ignored [yaml: %s:1, header: %s:1]",
                     base_path, yaml_key, info->name,
                     ctx->yaml_path ? ctx->yaml_path : "<unknown>",
                     ctx->header_path ? ctx->header_path : "<unknown>");
    } else {
        warn_yellowf("YAML field '%s' is not defined in header struct '%s' and will be ignored [yaml: %s:1, header: %s:1]",
                     yaml_key, info->name,
                     ctx->yaml_path ? ctx->yaml_path : "<unknown>",
                     ctx->header_path ? ctx->header_path : "<unknown>");
    }

    free(base_path);
}

static void gen_indent(gen_context* ctx) {
    for (int i = 0; i < ctx->indent; i++) {
        output_append(ctx->out, "    ");
    }
}

/* Forward declarations */
static bool gen_value(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                      const cg_field_info* field);
static bool gen_struct_value(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                             const cg_struct_info* info);

static bool starts_with(const char* s, const char* prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}

static char* extract_template_payload(const char* type, const char* wrapper) {
    if (!type || !wrapper) return NULL;
    const size_t wrapper_len = strlen(wrapper);
    if (!starts_with(type, wrapper) || type[wrapper_len] != '<') {
        return NULL;
    }

    const char* start = type + wrapper_len + 1;
    const char* p = start;
    int depth = 1;
    const char* end = NULL;
    while (*p) {
        if (*p == '<') depth++;
        else if (*p == '>') {
            depth--;
            if (depth == 0) {
                end = p;
                break;
            }
        }
        p++;
    }
    if (!end) return NULL;

    while (*start && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    return cg_strndup(start, (size_t)(end - start));
}

static bool extract_two_template_params(const char* type, const char* wrapper,
                                        char** out_a, char** out_b) {
    if (!out_a || !out_b) return false;
    *out_a = NULL;
    *out_b = NULL;

    char* payload = extract_template_payload(type, wrapper);
    if (!payload) return false;

    const char* comma = NULL;
    int depth = 0;
    for (const char* p = payload; *p; ++p) {
        if (*p == '<') depth++;
        else if (*p == '>') depth--;
        else if (*p == ',' && depth == 0) {
            comma = p;
            break;
        }
    }
    if (!comma) {
        free(payload);
        return false;
    }

    const char* a_start = payload;
    const char* a_end = comma;
    while (a_end > a_start && isspace((unsigned char)*(a_end - 1))) a_end--;
    while (*a_start && isspace((unsigned char)*a_start)) a_start++;

    const char* b_start = comma + 1;
    while (*b_start && isspace((unsigned char)*b_start)) b_start++;
    const char* b_end = payload + strlen(payload);
    while (b_end > b_start && isspace((unsigned char)*(b_end - 1))) b_end--;

    if (a_end <= a_start || b_end <= b_start) {
        free(payload);
        return false;
    }

    *out_a = cg_strndup(a_start, (size_t)(a_end - a_start));
    *out_b = cg_strndup(b_start, (size_t)(b_end - b_start));
    free(payload);

    if (!*out_a || !*out_b) {
        free(*out_a);
        free(*out_b);
        *out_a = NULL;
        *out_b = NULL;
        return false;
    }
    return true;
}

static void reset_field_derived_types(cg_field_info* field) {
    if (!field) return;
    field->is_array = false;
    field->is_optional = false;
    field->is_oneof = false;
    free(field->array_elem_type);
    free(field->optional_value_type);
    free(field->oneof_type_a);
    free(field->oneof_type_b);
    field->array_elem_type = NULL;
    field->optional_value_type = NULL;
    field->oneof_type_a = NULL;
    field->oneof_type_b = NULL;
}

static bool populate_field_type_traits(const cg_generator* gen, cg_field_info* field) {
    if (!gen || !field || !field->type) return false;
    reset_field_derived_types(field);

    field->array_elem_type = extract_template_payload(field->type, gen->array_wrapper);
    if (field->array_elem_type) {
        field->is_array = true;
    }

    field->optional_value_type = extract_template_payload(field->type, gen->optional_wrapper);
    if (field->optional_value_type) {
        field->is_optional = true;
    }

    if (extract_two_template_params(
            field->type, gen->oneof_wrapper, &field->oneof_type_a, &field->oneof_type_b)) {
        field->is_oneof = true;
    }

    return true;
}

static bool is_unsigned_integer_type(const char* type) {
    if (!type) return false;

    return strcmp(type, "unsigned int") == 0 ||
           strcmp(type, "unsigned long") == 0 ||
           strcmp(type, "unsigned long long") == 0 ||
           strcmp(type, "unsigned short") == 0 ||
           strcmp(type, "unsigned char") == 0 ||
           strcmp(type, "uint8_t") == 0 ||
           strcmp(type, "uint16_t") == 0 ||
           strcmp(type, "uint32_t") == 0 ||
           strcmp(type, "uint64_t") == 0;
}

static bool has_integer_literal_suffix(const char* value) {
    if (!value || !*value) return false;

    const char* p = value;
    if (*p == '+' || *p == '-') p++;
    while (*p >= '0' && *p <= '9') p++;

    return *p != '\0';
}

/**
 * @brief Generate scalar value.
 */
static bool gen_scalar(gen_context* ctx, const char* value, const cg_field_info* field) {
    const char* type = field ? field->type : "auto";
    yaml_value_type vtype = detect_yaml_type(value);

    /* Check for expression field */
    if (ctx->gen->has_expr_handler && field &&
        ctx->gen->expr_handler.is_expression_field(type, ctx->gen->expr_handler.userdata)) {
        char* path_str = cg_path_to_string(ctx->path);
        char* code = ctx->gen->expr_handler.generate_code(value, path_str, ctx->gen->expr_handler.userdata);
        free(path_str);

        if (!code) {
            cg_error_set(ctx->err, CG_ERR_EXPRESSION_ERROR, "Expression generation failed");
            return false;
        }

        output_append(ctx->out, code);
        free(code);
        return true;
    }

    /* Type-specific generation */
    if (strcmp(type, "bool") == 0) {
        output_append(ctx->out, yaml_to_bool(value) ? "true" : "false");
    } else if (strcmp(type, "int") == 0 || strcmp(type, "long") == 0 ||
               strcmp(type, "short") == 0 || strcmp(type, "int32_t") == 0 ||
               strcmp(type, "int64_t") == 0 || strcmp(type, "size_t") == 0) {
        output_append(ctx->out, value);
    } else if (is_unsigned_integer_type(type)) {
        output_append(ctx->out, value);
        if (vtype == YAML_VAL_INT && value[0] != '-' && !has_integer_literal_suffix(value)) {
            output_append(ctx->out, "ULL");
        }
    } else if (strcmp(type, "double") == 0 || strcmp(type, "float") == 0) {
        output_append(ctx->out, value);
        /* Add .0 if no decimal point */
        if (!strchr(value, '.') && !strchr(value, 'e') && !strchr(value, 'E')) {
            output_append(ctx->out, ".0");
        }
    } else if (strcmp(type, "std::string") == 0 || strcmp(type, "std::string_view") == 0) {
        output_append(ctx->out, "\"");
        /* Escape special characters */
        for (const char* p = value; *p; p++) {
            if (*p == '"') output_append(ctx->out, "\\\"");
            else if (*p == '\\') output_append(ctx->out, "\\\\");
            else if (*p == '\n') output_append(ctx->out, "\\n");
            else if (*p == '\t') output_append(ctx->out, "\\t");
            else {
                char c[2] = {*p, '\0'};
                output_append(ctx->out, c);
            }
        }
        output_append(ctx->out, "\"");
    } else {
        /* Default: treat as numeric or string based on YAML type */
        if (vtype == YAML_VAL_STRING) {
            output_append(ctx->out, "\"");
            output_append(ctx->out, value);
            output_append(ctx->out, "\"");
        } else if (vtype == YAML_VAL_BOOL) {
            output_append(ctx->out, yaml_to_bool(value) ? "true" : "false");
        } else {
            output_append(ctx->out, value);
        }
    }

    return true;
}

/**
 * @brief Generate array value with backing storage.
 */
static bool gen_array(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                      const cg_field_info* field) {
    if (node->type != YAML_SEQUENCE_NODE) {
        cg_error_set(ctx->err, CG_ERR_TYPE_MISMATCH, "Expected array");
        return false;
    }

    const char* elem_type = field->array_elem_type;
    char backing_name[64];
    snprintf(backing_name, sizeof(backing_name), "_backing_%d", ctx->backing_counter++);

    /* Generate backing array in backing storage section */
    output_appendf(ctx->backing, "static constexpr %s %s_data[] = {", elem_type, backing_name);

    yaml_node_item_t* item;
    bool first = true;
    size_t index = 0;
    for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
        yaml_node_t* elem = yaml_document_get_node(doc, *item);
        if (!first) output_append(ctx->backing, ", ");
        first = false;

        cg_path_push_index(ctx->path, index);

        /* Create temp field for element */
        cg_field_info elem_field = {0};
        elem_field.type = (char*)elem_type;
        if (!populate_field_type_traits(ctx->gen, &elem_field)) {
            cg_error_set(ctx->err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }

        /* Generate element value to backing storage */
        cg_output* saved = ctx->out;
        ctx->out = ctx->backing;

        bool ok = gen_value(ctx, doc, elem, &elem_field);

        ctx->out = saved;
        cg_path_pop(ctx->path);
        reset_field_derived_types(&elem_field);

        if (!ok) return false;
        index++;
    }

    output_append(ctx->backing, "};\n");
    output_appendf(ctx->backing, "static constexpr size_t %s_count = %zu;\n", backing_name, index);

    /* Reference backing array */
    output_appendf(
        ctx->out,
        ctx->gen->array_ctor_fmt,
        elem_type,
        backing_name,
        backing_name);
    return true;
}

/**
 * @brief Generate optional value.
 */
static bool gen_optional(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                         const cg_field_info* field) {
    const char* value_type = field->optional_value_type;

    if (node->type == YAML_SCALAR_NODE) {
        const char* value = (const char*)node->data.scalar.value;
        yaml_value_type vtype = detect_yaml_type(value);

        if (vtype == YAML_VAL_NULL) {
            output_appendf(ctx->out, ctx->gen->optional_empty_fmt, value_type);
            return true;
        }
    }

    /* Has value */
    output_appendf(ctx->out, ctx->gen->optional_value_prefix_fmt, value_type);

    cg_field_info inner_field = {0};
    inner_field.type = (char*)value_type;
    if (!populate_field_type_traits(ctx->gen, &inner_field)) {
        cg_error_set(ctx->err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    bool ok = gen_value(ctx, doc, node, &inner_field);
    reset_field_derived_types(&inner_field);
    output_append(ctx->out, ctx->gen->optional_value_suffix);
    return ok;
}

static bool is_struct_mapping_type(gen_context* ctx, const char* type) {
    return type && cg_struct_parser_find_struct(ctx->gen->parser, type) != NULL;
}

static bool gen_oneof(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                      const cg_field_info* field) {
    if (!field->oneof_type_a || !field->oneof_type_b) {
        cg_error_set(ctx->err, CG_ERR_PARSE_ERROR, "Invalid OneOf type");
        return false;
    }

    bool use_a = true;
    if (node->type == YAML_MAPPING_NODE) {
        const bool a_is_struct = is_struct_mapping_type(ctx, field->oneof_type_a);
        const bool b_is_struct = is_struct_mapping_type(ctx, field->oneof_type_b);
        if (!a_is_struct && b_is_struct) use_a = false;
    } else if (node->type == YAML_SCALAR_NODE) {
        const bool a_is_struct = is_struct_mapping_type(ctx, field->oneof_type_a);
        const bool b_is_struct = is_struct_mapping_type(ctx, field->oneof_type_b);
        if (a_is_struct && !b_is_struct) use_a = false;
    }

    cg_field_info inner = {0};
    inner.type = use_a ? field->oneof_type_a : field->oneof_type_b;
    if (!populate_field_type_traits(ctx->gen, &inner)) {
        cg_error_set(ctx->err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    const bool inner_is_struct = is_struct_mapping_type(ctx, inner.type);
    output_appendf(ctx->out, "%s{std::in_place_index<%d>, ", field->type, use_a ? 0 : 1);
    if (inner_is_struct) {
        output_appendf(ctx->out, "%s", inner.type);
    }
    bool ok = gen_value(ctx, doc, node, &inner);
    output_append(ctx->out, "}");
    reset_field_derived_types(&inner);
    return ok;
}

/**
 * @brief Generate a value based on field type.
 */
static bool gen_value(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                      const cg_field_info* field) {
    if (!node || !field) return false;

    cg_field_info derived = {0};
    derived.type = field->type;
    if (!populate_field_type_traits(ctx->gen, &derived)) {
        cg_error_set(ctx->err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    /* Handle array */
    if (derived.is_array) {
        const bool ok = gen_array(ctx, doc, node, &derived);
        reset_field_derived_types(&derived);
        return ok;
    }

    /* Handle optional */
    if (derived.is_optional) {
        const bool ok = gen_optional(ctx, doc, node, &derived);
        reset_field_derived_types(&derived);
        return ok;
    }

    /* Handle OneOf */
    if (derived.is_oneof) {
        const bool ok = gen_oneof(ctx, doc, node, &derived);
        reset_field_derived_types(&derived);
        return ok;
    }

    /* Handle scalar */
    if (node->type == YAML_SCALAR_NODE) {
        const bool ok = gen_scalar(ctx, (const char*)node->data.scalar.value, field);
        reset_field_derived_types(&derived);
        return ok;
    }

    /* Handle nested struct (mapping) */
    if (node->type == YAML_MAPPING_NODE) {
        const cg_struct_info* nested = cg_struct_parser_find_struct(ctx->gen->parser, field->type);
        if (!nested) {
            reset_field_derived_types(&derived);
            cg_error_set(ctx->err, CG_ERR_UNKNOWN_STRUCT, "Unknown struct type");
            return false;
        }
        const bool ok = gen_struct_value(ctx, doc, node, nested);
        reset_field_derived_types(&derived);
        return ok;
    }

    reset_field_derived_types(&derived);
    cg_error_set(ctx->err, CG_ERR_TYPE_MISMATCH, "Unexpected YAML node type");
    return false;
}

/**
 * @brief Generate struct initialization.
 */
static bool gen_struct_value(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                             const cg_struct_info* info) {
    if (node->type != YAML_MAPPING_NODE) {
        cg_error_set(ctx->err, CG_ERR_TYPE_MISMATCH, "Expected mapping for struct");
        return false;
    }

    output_append(ctx->out, "{\n");
    ctx->indent++;

    const size_t pair_count =
        (size_t)(node->data.mapping.pairs.top - node->data.mapping.pairs.start);
    bool* pair_used = (bool*)calloc(pair_count, sizeof(bool));
    if (!pair_used) {
        cg_error_set(ctx->err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    /* Process each field in struct order */
    for (size_t i = 0; i < info->field_count; i++) {
        const cg_field_info* field = &info->fields[i];

        /* Find corresponding YAML key */
        char* snake_name = cg_to_snake_case(ctx->gen->utils, field->name);
        yaml_node_t* value_node = NULL;

        yaml_node_pair_t* pair;
        size_t pair_index = 0;
        for (pair = node->data.mapping.pairs.start;
             pair < node->data.mapping.pairs.top; pair++, pair_index++) {
            yaml_node_t* key_node = yaml_document_get_node(doc, pair->key);
            if (key_node && key_node->type == YAML_SCALAR_NODE) {
                if (strcmp((const char*)key_node->data.scalar.value, snake_name) == 0 ||
                    strcmp((const char*)key_node->data.scalar.value, field->name) == 0) {
                    value_node = yaml_document_get_node(doc, pair->value);
                    pair_used[pair_index] = true;
                    break;
                }
            }
        }

        free(snake_name);

        gen_indent(ctx);

        if (value_node) {
            cg_path_push(ctx->path, field->name);
            bool ok = gen_value(ctx, doc, value_node, field);
            cg_path_pop(ctx->path);
            if (!ok) {
                free(pair_used);
                return false;
            }
        } else if (field->default_value) {
            /* Use default value */
            warn_missing_header_field(ctx, info, field, "Using default value");
            output_append(ctx->out, field->default_value);
        } else {
            cg_field_info derived = {0};
            derived.type = field->type;
            const bool have_traits = populate_field_type_traits(ctx->gen, &derived);
            const bool is_optional = have_traits && derived.is_optional;
            char* optional_type = is_optional ? cg_strdup(derived.optional_value_type) : NULL;
            reset_field_derived_types(&derived);

            if (!have_traits) {
                free(pair_used);
                cg_error_set(ctx->err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
                return false;
            }

            if (is_optional) {
            /* Optional without value -> empty */
            warn_missing_header_field(ctx, info, field, "Using Optional<...>::empty()");
                output_appendf(ctx->out, ctx->gen->optional_empty_fmt, optional_type);
                free(optional_type);
            } else {
            /* Use zero/empty initialization */
            warn_missing_header_field(ctx, info, field, "Using {} initialization");
            output_append(ctx->out, "{}");
            }
        }

        if (i < info->field_count - 1) {
            output_append(ctx->out, ",");
        }
        output_appendf(ctx->out, "  // %s\n", field->name);
    }

    /* Warn for YAML keys that do not map to a known field in this struct. */
    {
        yaml_node_pair_t* pair;
        size_t pair_index = 0;
        for (pair = node->data.mapping.pairs.start;
             pair < node->data.mapping.pairs.top; pair++, pair_index++) {
            if (pair_used[pair_index]) continue;

            yaml_node_t* key_node = yaml_document_get_node(doc, pair->key);
            if (!key_node || key_node->type != YAML_SCALAR_NODE) continue;

            warn_unknown_yaml_key(ctx, info, (const char*)key_node->data.scalar.value);
        }
    }

    free(pair_used);
    ctx->indent--;
    gen_indent(ctx);
    output_append(ctx->out, "}");
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

cg_generator* cg_generator_new(const cg_struct_parser* parser, const cg_string_utils* utils) {
    cg_generator* gen = (cg_generator*)calloc(1, sizeof(cg_generator));
    if (!gen) return NULL;

    gen->parser = parser;
    gen->utils = utils;
    gen->has_expr_handler = false;
    gen->array_wrapper = cg_strdup("Array");
    gen->optional_wrapper = cg_strdup("Optional");
    gen->oneof_wrapper = cg_strdup("OneOf");
    gen->array_ctor_fmt = cg_strdup("Array<%s>{%s_data, %s_count}");
    gen->optional_empty_fmt = cg_strdup("Optional<%s>::empty()");
    gen->optional_value_prefix_fmt = cg_strdup("Optional<%s>{");
    gen->optional_value_suffix = cg_strdup("}");

    if (!gen->array_wrapper || !gen->optional_wrapper || !gen->oneof_wrapper ||
        !gen->array_ctor_fmt || !gen->optional_empty_fmt ||
        !gen->optional_value_prefix_fmt || !gen->optional_value_suffix) {
        cg_generator_free(gen);
        return NULL;
    }
    return gen;
}

void cg_generator_free(cg_generator* gen) {
    if (!gen) return;
    free(gen->array_wrapper);
    free(gen->optional_wrapper);
    free(gen->oneof_wrapper);
    free(gen->array_ctor_fmt);
    free(gen->optional_empty_fmt);
    free(gen->optional_value_prefix_fmt);
    free(gen->optional_value_suffix);
    free(gen);
}

void cg_generator_set_expression_handler(cg_generator* gen, const cg_expression_handler* handler) {
    if (!gen || !handler) return;
    gen->expr_handler = *handler;
    gen->has_expr_handler = true;
}

void cg_generator_set_type_options(cg_generator* gen, const cg_type_options* options) {
    if (!gen || !options) return;

    char* next_array_wrapper = options->array_wrapper ? cg_strdup(options->array_wrapper) : NULL;
    char* next_optional_wrapper = options->optional_wrapper ? cg_strdup(options->optional_wrapper) : NULL;
    char* next_oneof_wrapper = options->oneof_wrapper ? cg_strdup(options->oneof_wrapper) : NULL;
    char* next_array_ctor_fmt = options->array_ctor_fmt ? cg_strdup(options->array_ctor_fmt) : NULL;
    char* next_optional_empty_fmt = options->optional_empty_fmt ? cg_strdup(options->optional_empty_fmt) : NULL;
    char* next_optional_value_prefix_fmt =
        options->optional_value_prefix_fmt ? cg_strdup(options->optional_value_prefix_fmt) : NULL;
    char* next_optional_value_suffix =
        options->optional_value_suffix ? cg_strdup(options->optional_value_suffix) : NULL;

    if ((options->array_wrapper && !next_array_wrapper) ||
        (options->optional_wrapper && !next_optional_wrapper) ||
        (options->oneof_wrapper && !next_oneof_wrapper) ||
        (options->array_ctor_fmt && !next_array_ctor_fmt) ||
        (options->optional_empty_fmt && !next_optional_empty_fmt) ||
        (options->optional_value_prefix_fmt && !next_optional_value_prefix_fmt) ||
        (options->optional_value_suffix && !next_optional_value_suffix)) {
        free(next_array_wrapper);
        free(next_optional_wrapper);
        free(next_oneof_wrapper);
        free(next_array_ctor_fmt);
        free(next_optional_empty_fmt);
        free(next_optional_value_prefix_fmt);
        free(next_optional_value_suffix);
        return;
    }

    if (next_array_wrapper) {
        free(gen->array_wrapper);
        gen->array_wrapper = next_array_wrapper;
    }
    if (next_optional_wrapper) {
        free(gen->optional_wrapper);
        gen->optional_wrapper = next_optional_wrapper;
    }
    if (next_oneof_wrapper) {
        free(gen->oneof_wrapper);
        gen->oneof_wrapper = next_oneof_wrapper;
    }
    if (next_array_ctor_fmt) {
        free(gen->array_ctor_fmt);
        gen->array_ctor_fmt = next_array_ctor_fmt;
    }
    if (next_optional_empty_fmt) {
        free(gen->optional_empty_fmt);
        gen->optional_empty_fmt = next_optional_empty_fmt;
    }
    if (next_optional_value_prefix_fmt) {
        free(gen->optional_value_prefix_fmt);
        gen->optional_value_prefix_fmt = next_optional_value_prefix_fmt;
    }
    if (next_optional_value_suffix) {
        free(gen->optional_value_suffix);
        gen->optional_value_suffix = next_optional_value_suffix;
    }
}

static cg_output* cg_generate_from_document(cg_generator* gen,
                                            yaml_document_t* doc,
                                            const char* yaml_path,
                                            const char* header_path,
                                            cg_error* err) {
    yaml_node_t* root = yaml_document_get_root_node(doc);
    if (!root || root->type != YAML_MAPPING_NODE) {
        cg_error_set(err, CG_ERR_YAML_ERROR, "YAML root must be a mapping");
        return NULL;
    }

    /* Create output buffers */
    cg_output* out = output_new();
    cg_output* backing = output_new();
    cg_path* path = cg_path_new();

    if (!out || !backing || !path) {
        cg_output_free(out);
        cg_output_free(backing);
        cg_path_free(path);
        cg_error_set(err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    /* Setup generation context */
    gen_context ctx = {0};
    ctx.gen = gen;
    ctx.out = out;
    ctx.backing = backing;
    ctx.path = path;
    ctx.err = err;
    ctx.yaml_path = yaml_path;
    ctx.header_path = header_path;
    ctx.indent = 0;
    ctx.backing_counter = 0;

    /* Generate backing storage */
    output_append(backing, "namespace {\n");

    /* Find root struct (last struct in parser - typically the main config struct) */
    const cg_struct_info* root_struct = NULL;
    size_t struct_count = cg_struct_parser_get_struct_count(gen->parser);
    if (struct_count > 0) {
        root_struct = cg_struct_parser_get_struct(gen->parser, struct_count - 1);
    }

    if (!root_struct) {
        cg_output_free(out);
        cg_output_free(backing);
        cg_path_free(path);
        cg_error_set(err, CG_ERR_UNKNOWN_STRUCT, "No struct found in header");
        return NULL;
    }

    /* Generate config initialization */
    output_appendf(out, "constexpr %s config = ", root_struct->name);

    cg_path_push(path, root_struct->name);
    bool ok = gen_struct_value(&ctx, doc, root, root_struct);
    cg_path_pop(path);

    output_append(out, ";\n");
    cg_path_free(path);

    if (!ok) {
        cg_output_free(out);
        cg_output_free(backing);
        return NULL;
    }

    /* Combine backing storage and main output */
    output_append(backing, "} // namespace\n\n");

    cg_output* final = output_new();
    if (!final) {
        cg_output_free(out);
        cg_output_free(backing);
        cg_error_set(err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    output_append(final, backing->code);
    output_append(final, out->code);

    cg_output_free(out);
    cg_output_free(backing);
    return final;
}

cg_output* cg_generate(cg_generator* gen, const char* yaml_path,
                       const char* header_path, cg_error* err) {
    cg_error_init(err);
    if (!gen || !yaml_path || !header_path) {
        cg_error_set(err, CG_ERR_PARSE_ERROR, "Invalid arguments");
        return NULL;
    }

    FILE* f = fopen(yaml_path, "r");
    if (!f) {
        cg_error_set(err, CG_ERR_FILE_NOT_FOUND, "Cannot open YAML file");
        return NULL;
    }

    yaml_parser_t parser;
    yaml_document_t doc;
    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        cg_error_set(err, CG_ERR_YAML_ERROR, "Failed to initialize YAML parser");
        return NULL;
    }
    yaml_parser_set_input_file(&parser, f);

    if (!yaml_parser_load(&parser, &doc)) {
        cg_error_set(err, CG_ERR_YAML_ERROR, "Failed to parse YAML");
        yaml_parser_delete(&parser);
        fclose(f);
        return NULL;
    }

    cg_output* result = cg_generate_from_document(gen, &doc, yaml_path, header_path, err);

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    fclose(f);
    return result;
}

cg_output* cg_generate_from_yaml_text(cg_generator* gen,
                                      const char* yaml_text,
                                      const char* yaml_virtual_path,
                                      const char* header_path,
                                      cg_error* err) {
    cg_error_init(err);
    if (!gen || !yaml_text || !header_path) {
        cg_error_set(err, CG_ERR_PARSE_ERROR, "Invalid arguments");
        return NULL;
    }

    yaml_parser_t parser;
    yaml_document_t doc;
    if (!yaml_parser_initialize(&parser)) {
        cg_error_set(err, CG_ERR_YAML_ERROR, "Failed to initialize YAML parser");
        return NULL;
    }
    yaml_parser_set_input_string(
        &parser, (const unsigned char*)yaml_text, strlen(yaml_text));

    if (!yaml_parser_load(&parser, &doc)) {
        cg_error_set(err, CG_ERR_YAML_ERROR, "Failed to parse YAML");
        yaml_parser_delete(&parser);
        return NULL;
    }

    const char* path_for_diag =
        (yaml_virtual_path && yaml_virtual_path[0] != '\0') ? yaml_virtual_path : "<in-memory-yaml>";
    cg_output* result = cg_generate_from_document(gen, &doc, path_for_diag, header_path, err);

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    return result;
}

const char* cg_output_get_code(const cg_output* output) {
    return output ? output->code : NULL;
}

size_t cg_output_get_code_length(const cg_output* output) {
    return output ? output->length : 0;
}

void cg_output_free(cg_output* output) {
    if (!output) return;
    free(output->code);
    free(output);
}
