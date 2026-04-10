/**
 * @file code_generator.c
 * @brief Generate pure C11 headers from YAML configuration.
 */

#include "internal.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <yaml.h>

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

static void generated_field_cleanup(cxgn_field_info* field) {
    if (!field) return;
    free(field->name);
    free(field->type);
    free(field->default_value);
    free(field->array_elem_type);
    free(field->optional_value_type);
    for (size_t i = 0; i < field->variant_type_count; i++) free(field->variant_types[i]);
    free(field->variant_types);
    memset(field, 0, sizeof(*field));
}

static cxgn_output* output_new(void) {
    cxgn_output* out = (cxgn_output*)calloc(1, sizeof(*out));
    if (!out) return NULL;
    out->capacity = CXGN_BUFFER_SIZE;
    out->code = (char*)malloc(out->capacity);
    if (!out->code) {
        free(out);
        return NULL;
    }
    out->code[0] = '\0';
    return out;
}

void cxgn_output_free(cxgn_output* output) {
    if (!output) return;
    free(output->code);
    free(output);
}

const char* cxgn_output_get_code(const cxgn_output* output) {
    return output ? output->code : NULL;
}

size_t cxgn_output_get_code_length(const cxgn_output* output) {
    return output ? output->length : 0;
}

static bool output_append(cxgn_output* out, const char* str) {
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

static bool output_appendf(cxgn_output* out, const char* fmt, ...) {
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

    const bool ok = output_append(out, buf);
    free(buf);
    return ok;
}

static void gen_indent(gen_context* ctx) {
    for (int i = 0; i < ctx->indent; i++) output_append(ctx->out, "    ");
}

static yaml_value_type detect_yaml_type(const char* value) {
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

static bool yaml_to_bool(const char* value) {
    return strcmp(value, "true") == 0;
}

static bool is_unsigned_integer_type(const char* type) {
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

static bool has_integer_literal_suffix(const char* value) {
    if (!value || !*value) return false;
    const char* p = value;
    if (*p == '+' || *p == '-') p++;
    while (*p >= '0' && *p <= '9') p++;
    return *p != '\0';
}

static bool unsigned_integer_literal_needs_suffix(const char* value) {
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

static char* escape_string(const char* value) {
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

static char* make_backing_name(gen_context* ctx) {
    char* path = cxgn_path_to_string(ctx->path);
    if (!path) return NULL;
    for (char* p = path; *p; p++) {
        if (*p == '.' || *p == '[' || *p == ']') *p = '_';
    }
    const char* pfx = ctx->gen->symbol_prefix ? ctx->gen->symbol_prefix : "";
    char buf[512];
    snprintf(buf, sizeof(buf), "_%sbacking_%s_%d", pfx, path[0] ? path : "value", ctx->backing_counter++);
    free(path);
    return cxgn_strdup(buf);
}

static bool populate_field_alias_traits(const cxgn_struct_parser* parser, cxgn_field_info* field) {
    const cxgn_type_alias* alias;
    if (!field || !field->type) return true;
    alias = cxgn_struct_parser_find_alias(parser, field->type);
    if (!alias) return true;
    if (alias->kind == CXGN_ALIAS_SCALAR) {
        /* Resolve the typedef to its underlying type so gen_scalar handles it correctly. */
        char* resolved = cxgn_strdup(alias->value_type);
        if (!resolved) return false;
        free(field->type);
        field->type = resolved;
        /* Re-enter to resolve chained aliases (e.g. typedef expr_t my_t). */
        return populate_field_alias_traits(parser, field);
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

static bool gen_value(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                      const cxgn_field_info* field);

static bool gen_scalar(gen_context* ctx, const char* value, const cxgn_field_info* field) {
    yaml_value_type vtype = detect_yaml_type(value);
    const char* type = field ? field->type : NULL;

    if (ctx->gen->has_expr_handler && type &&
        ctx->gen->expr_handler.is_expression_field &&
        ctx->gen->expr_handler.is_expression_field(type, ctx->gen->expr_handler.userdata)) {
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
        output_append(ctx->out, code);
        free(code);
        free(yaml_path);
        return true;
    }

    if (type && strcmp(type, "bool") == 0) {
        return output_append(ctx->out, yaml_to_bool(value) ? "true" : "false");
    }
    if (type && is_unsigned_integer_type(type) && vtype == YAML_VAL_INT) {
        output_append(ctx->out, value);
        if (!has_integer_literal_suffix(value) && unsigned_integer_literal_needs_suffix(value)) {
            output_append(ctx->out, "ULL");
        }
        return true;
    }
    if (type && (strcmp(type, "float") == 0 || strcmp(type, "double") == 0 || strcmp(type, "long double") == 0)) {
        output_append(ctx->out, value);
        if (vtype == YAML_VAL_INT) output_append(ctx->out, ".0");
        return true;
    }
    if (type &&
        (strcmp(type, "const char*") == 0 ||
         strcmp(type, "std::string") == 0 ||
         strcmp(type, "std::string_view") == 0)) {
        char* escaped = escape_string(value);
        if (!escaped) return false;
        const bool ok = output_appendf(ctx->out, "\"%s\"", escaped);
        free(escaped);
        return ok;
    }

    if (vtype == YAML_VAL_STRING) {
        char* escaped = escape_string(value);
        if (!escaped) return false;
        const bool ok = output_appendf(ctx->out, "\"%s\"", escaped);
        free(escaped);
        return ok;
    }
    if (vtype == YAML_VAL_BOOL) return output_append(ctx->out, yaml_to_bool(value) ? "true" : "false");
    return output_append(ctx->out, value);
}

static bool gen_struct_value(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                             const cxgn_struct_info* info) {
    if (node->type != YAML_MAPPING_NODE) {
        cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Expected mapping for struct");
        return false;
    }

    output_append(ctx->out, "{\n");
    ctx->indent++;

    for (size_t i = 0; i < info->field_count; i++) {
        const cxgn_field_info* field = &info->fields[i];
        char* snake = cxgn_to_snake_case(ctx->gen->utils, field->name);
        yaml_node_t* value_node = NULL;

        for (yaml_node_pair_t* pair = node->data.mapping.pairs.start;
             pair < node->data.mapping.pairs.top; pair++) {
            yaml_node_t* key = yaml_document_get_node(doc, pair->key);
            if (!key || key->type != YAML_SCALAR_NODE) continue;
            const char* key_text = (const char*)key->data.scalar.value;
            if (strcmp(key_text, field->name) == 0 || (snake && strcmp(key_text, snake) == 0)) {
                value_node = yaml_document_get_node(doc, pair->value);
                break;
            }
        }
        free(snake);

        gen_indent(ctx);
        output_appendf(ctx->out, ".%s = ", field->name);
        if (value_node) {
            cxgn_path_push(ctx->path, field->name);
            if (!gen_value(ctx, doc, value_node, field)) {
                cxgn_path_pop(ctx->path);
                return false;
            }
            cxgn_path_pop(ctx->path);
        } else if (field->is_optional) {
            output_append(ctx->out, "{.has_value = false}");
        } else {
            output_append(ctx->out, "{0}");
        }

        if (i + 1 < info->field_count) output_append(ctx->out, ",");
        output_append(ctx->out, "\n");
    }

    ctx->indent--;
    gen_indent(ctx);
    output_append(ctx->out, "}");
    return true;
}

static bool gen_array(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                      const cxgn_field_info* field) {
    if (node->type != YAML_SEQUENCE_NODE) {
        cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Expected sequence for array");
        return false;
    }

    char* backing_name = make_backing_name(ctx);
    cxgn_output* temp = output_new();
    if (!backing_name || !temp) {
        free(backing_name);
        cxgn_output_free(temp);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    size_t count = 0;
    for (yaml_node_item_t* item = node->data.sequence.items.start;
         item < node->data.sequence.items.top; item++, count++) {
        yaml_node_t* elem = yaml_document_get_node(doc, *item);
        if (count > 0) output_append(temp, ", ");
        cxgn_field_info elem_field = {0};
        elem_field.type = cxgn_strdup(field->array_elem_type);
        if (!elem_field.type || !populate_field_alias_traits(ctx->gen->parser, &elem_field)) {
            generated_field_cleanup(&elem_field);
            cxgn_output_free(temp);
            free(backing_name);
            cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }
        cxgn_output* saved = ctx->out;
        ctx->out = temp;
        cxgn_path_push_index(ctx->path, count);
        const bool ok = gen_value(ctx, doc, elem, &elem_field);
        cxgn_path_pop(ctx->path);
        ctx->out = saved;
        generated_field_cleanup(&elem_field);
        if (!ok) {
            cxgn_output_free(temp);
            free(backing_name);
            return false;
        }
    }

    output_appendf(ctx->backing, "static const %s %s_data[] = {%s};\n",
                   field->array_elem_type, backing_name, temp->code);
    output_appendf(ctx->out, "{.data = %s_data, .count = %zu}", backing_name, count);

    cxgn_output_free(temp);
    free(backing_name);
    return true;
}

static bool gen_optional(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                         const cxgn_field_info* field) {
    if (node->type == YAML_SCALAR_NODE &&
        detect_yaml_type((const char*)node->data.scalar.value) == YAML_VAL_NULL) {
        return output_append(ctx->out, "{.has_value = false}");
    }

    output_append(ctx->out, "{.value = ");
    cxgn_field_info inner = {0};
    inner.type = cxgn_strdup(field->optional_value_type);
    if (!inner.type || !populate_field_alias_traits(ctx->gen->parser, &inner)) {
        generated_field_cleanup(&inner);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }
    const bool ok = gen_value(ctx, doc, node, &inner);
    generated_field_cleanup(&inner);
    output_append(ctx->out, ", .has_value = true}");
    return ok;
}

static bool gen_value(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                      const cxgn_field_info* field) {
    cxgn_field_info derived = {0};
    derived.type = cxgn_strdup(field->type);
    derived.is_array = field->is_array;
    derived.is_optional = field->is_optional;
    derived.is_variant = field->is_variant;
    derived.array_elem_type = field->array_elem_type ? cxgn_strdup(field->array_elem_type) : NULL;
    derived.optional_value_type = field->optional_value_type ? cxgn_strdup(field->optional_value_type) : NULL;
    if (!derived.type || (field->array_elem_type && !derived.array_elem_type) ||
        (field->optional_value_type && !derived.optional_value_type)) {
        generated_field_cleanup(&derived);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }
    if (!populate_field_alias_traits(ctx->gen->parser, &derived)) {
        generated_field_cleanup(&derived);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    if (derived.is_variant) {
        generated_field_cleanup(&derived);
        cxgn_error_set(ctx->err, CXGN_ERR_UNKNOWN_TYPE, "Tagged unions are not implemented in pure-C mode");
        return false;
    }
    if (derived.is_array) {
        const bool ok = gen_array(ctx, doc, node, &derived);
        generated_field_cleanup(&derived);
        return ok;
    }
    if (derived.is_optional) {
        const bool ok = gen_optional(ctx, doc, node, &derived);
        generated_field_cleanup(&derived);
        return ok;
    }
    if (node->type == YAML_SCALAR_NODE) {
        const bool ok = gen_scalar(ctx, (const char*)node->data.scalar.value, &derived);
        generated_field_cleanup(&derived);
        return ok;
    }
    if (node->type == YAML_MAPPING_NODE) {
        const cxgn_struct_info* nested = cxgn_struct_parser_find_struct(ctx->gen->parser, field->type);
        if (!nested) {
            generated_field_cleanup(&derived);
            cxgn_error_set(ctx->err, CXGN_ERR_UNKNOWN_STRUCT, "Unknown struct type");
            return false;
        }
        const bool ok = gen_struct_value(ctx, doc, node, nested);
        generated_field_cleanup(&derived);
        return ok;
    }

    generated_field_cleanup(&derived);
    cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Unexpected YAML node type");
    return false;
}

cxgn_generator* cxgn_generator_new(const cxgn_struct_parser* parser, const cxgn_string_utils* utils) {
    cxgn_generator* gen = (cxgn_generator*)calloc(1, sizeof(*gen));
    if (!gen) return NULL;
    gen->parser = parser;
    gen->utils = utils;
    gen->array_wrapper = cxgn_strdup("cxgn_array");
    gen->optional_wrapper = cxgn_strdup("cxgn_optional");
    gen->variant_wrapper = cxgn_strdup("cxgn_variant");
    gen->array_ctor_fmt = cxgn_strdup("");
    gen->optional_empty_fmt = cxgn_strdup("");
    gen->optional_value_prefix_fmt = cxgn_strdup("");
    gen->optional_value_suffix = cxgn_strdup("");
    gen->cpp_std = CXGN_CPP_STD_20;
    if (!gen->array_wrapper || !gen->optional_wrapper || !gen->variant_wrapper ||
        !gen->array_ctor_fmt || !gen->optional_empty_fmt ||
        !gen->optional_value_prefix_fmt || !gen->optional_value_suffix) {
        cxgn_generator_free(gen);
        return NULL;
    }
    return gen;
}

void cxgn_generator_free(cxgn_generator* gen) {
    if (!gen) return;
    free(gen->helpers_header);
    free(gen->symbol_prefix);
    free(gen->array_wrapper);
    free(gen->optional_wrapper);
    free(gen->variant_wrapper);
    free(gen->array_ctor_fmt);
    free(gen->optional_empty_fmt);
    free(gen->optional_value_prefix_fmt);
    free(gen->optional_value_suffix);
    free(gen);
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

static bool emit_helper_typedefs(cxgn_output* out, const cxgn_generator* gen) {
    if (gen->helpers_header) return output_appendf(out, "#include <%s>\n\n", gen->helpers_header);
    return true;
}

static cxgn_output* cxgn_generate_from_document(cxgn_generator* gen, yaml_document_t* doc,
                                                const char* yaml_path, const char* header_path,
                                                cxgn_error* err) {
    yaml_node_t* root = yaml_document_get_root_node(doc);
    if (!root || root->type != YAML_MAPPING_NODE) {
        cxgn_error_set(err, CXGN_ERR_YAML_ERROR, "YAML root must be a mapping");
        return NULL;
    }

    const size_t count = cxgn_struct_parser_get_struct_count(gen->parser);
    const cxgn_struct_info* root_struct = count ? cxgn_struct_parser_get_struct(gen->parser, count - 1) : NULL;
    if (!root_struct) {
        cxgn_error_set(err, CXGN_ERR_UNKNOWN_STRUCT, "No struct found in header");
        return NULL;
    }

    cxgn_output* out = output_new();
    cxgn_output* backing = output_new();
    cxgn_path* path = cxgn_path_new();
    if (!out || !backing || !path) {
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_path_free(path);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    if (!emit_helper_typedefs(out, gen)) {
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
    output_appendf(out, "static const %s %sconfig = ", root_struct->name, sym_pfx);
    if (!gen_struct_value(&ctx, doc, root, root_struct)) {
        cxgn_path_free(path);
        cxgn_output_free(out);
        cxgn_output_free(backing);
        return NULL;
    }
    cxgn_path_pop(path);
    output_append(out, ";\n");

    cxgn_output* final = output_new();
    if (!final) {
        cxgn_path_free(path);
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    output_append(final, backing->code);
    if (backing->length > 0) output_append(final, "\n");
    output_append(final, out->code);

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

    FILE* f = fopen(yaml_path, "r");
    if (!f) {
        cxgn_error_set(err, CXGN_ERR_FILE_NOT_FOUND, "Cannot open YAML file");
        return NULL;
    }

    yaml_parser_t parser;
    yaml_document_t doc;
    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        cxgn_error_set(err, CXGN_ERR_YAML_ERROR, "Failed to initialize YAML parser");
        return NULL;
    }
    yaml_parser_set_input_file(&parser, f);
    if (!yaml_parser_load(&parser, &doc)) {
        yaml_parser_delete(&parser);
        fclose(f);
        cxgn_error_set(err, CXGN_ERR_YAML_ERROR, "Failed to parse YAML");
        return NULL;
    }

    cxgn_output* result = cxgn_generate_from_document(gen, &doc, yaml_path, header_path, err);
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    fclose(f);
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

    yaml_parser_t parser;
    yaml_document_t doc;
    if (!yaml_parser_initialize(&parser)) {
        cxgn_error_set(err, CXGN_ERR_YAML_ERROR, "Failed to initialize YAML parser");
        return NULL;
    }
    yaml_parser_set_input_string(&parser, (const unsigned char*)yaml_text, strlen(yaml_text));
    if (!yaml_parser_load(&parser, &doc)) {
        yaml_parser_delete(&parser);
        cxgn_error_set(err, CXGN_ERR_YAML_ERROR, "Failed to parse YAML");
        return NULL;
    }

    const char* diag_path = (yaml_virtual_path && yaml_virtual_path[0]) ? yaml_virtual_path : "<in-memory-yaml>";
    cxgn_output* result = cxgn_generate_from_document(gen, &doc, diag_path, header_path, err);
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    return result;
}
