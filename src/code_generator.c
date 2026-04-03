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

static cxgn_output* output_new(void) {
    cxgn_output* out = (cxgn_output*)calloc(1, sizeof(cxgn_output));
    if (!out) return NULL;

    out->capacity = CXGN_BUFFER_SIZE;
    out->code = (char*)malloc(out->capacity);
    if (!out->code) {
        free(out);
        return NULL;
    }
    out->code[0] = '\0';
    out->length = 0;
    return out;
}

static bool output_append(cxgn_output* out, const char* str) {
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

static bool output_appendf(cxgn_output* out, const char* fmt, ...) {
    char buf[CXGN_LINE_SIZE];
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

#define CXGN_MAX_SPOOL      256
#define CXGN_MAX_ARR_CACHE  256
#define CXGN_MAX_USED_NAMES 512
#define CXGN_MAX_BACKING_DECL 512

typedef enum {
    BACKING_DECL_SPOOL = 0,
    BACKING_DECL_ARRAY = 1
} backing_decl_kind;

typedef struct {
    backing_decl_kind kind;
    int sort_rank;
    char* name;
    char* code;
} backing_decl;

typedef struct {
    cxgn_generator* gen;
    cxgn_output* out;
    cxgn_output* backing;  /* Backing storage for arrays */
    cxgn_path* path;
    cxgn_error* err;
    const char* yaml_path;
    const char* header_path;
    int indent;
    int backing_counter;  /* Fallback counter for collision resolution */

    /* String pool: deduplicate identical string literals */
    char* spool_raw[CXGN_MAX_SPOOL];      /* Raw (unescaped) string value */
    char* spool_escaped[CXGN_MAX_SPOOL];  /* C-escaped form for pool declaration */
    char* spool_name[CXGN_MAX_SPOOL];     /* Pool variable name, e.g. _spool_0 */
    int   spool_count;

    /* Array content cache: deduplicate identical array backings */
    char* arr_elem_type[CXGN_MAX_ARR_CACHE]; /* Element type string */
    char* arr_content[CXGN_MAX_ARR_CACHE];   /* Rendered element list */
    char* arr_name[CXGN_MAX_ARR_CACHE];      /* Backing variable name */
    int   arr_count;

    /* Used backing names for collision detection */
    char* used_names[CXGN_MAX_USED_NAMES];
    int   used_count;

    /* Deferred backing declarations for sorted namespace emission */
    backing_decl decls[CXGN_MAX_BACKING_DECL];
    int decl_count;

    /* Set when a backing array is emitted as 'const' (not 'constexpr') */
    bool has_non_constexpr_backing;
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

static void warn_missing_header_field(gen_context* ctx, const cxgn_struct_info* info,
                                      const cxgn_field_info* field, const char* reason) {
    char* base_path = cxgn_path_to_string(ctx->path);
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

static void warn_unknown_yaml_key(gen_context* ctx, const cxgn_struct_info* info,
                                  const char* yaml_key) {
    char* base_path = cxgn_path_to_string(ctx->path);
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
                      const cxgn_field_info* field);
static bool gen_struct_value(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                             const cxgn_struct_info* info);
static bool emit_usage_comments(cxgn_output* out, const cxgn_struct_parser* parser,
                                const cxgn_struct_info* root_struct);
static char* extract_template_payload(const char* type, const char* wrapper);
static bool extract_variant_params(const char* type, const char* wrapper,
                                   char*** out_types, size_t* out_count);

static bool starts_with(const char* s, const char* prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}

static const cxgn_field_info* first_field_of_struct(const cxgn_struct_parser* parser,
                                                  const char* struct_name) {
    const cxgn_struct_info* info =
        (parser && struct_name) ? cxgn_struct_parser_find_struct(parser, struct_name) : NULL;
    if (!info || info->field_count == 0) return NULL;
    return &info->fields[0];
}

static bool emit_scalar_usage_comment(cxgn_output* out, const cxgn_field_info* field) {
    if (strcmp(field->type, "std::string") == 0) {
        return output_appendf(out,
                              "//   %s s = config.%s;\n"
                              "//   const char* cstr = config.%s.c_str();\n",
                              field->type, field->name, field->name);
    }

    if (strcmp(field->type, "std::string_view") == 0) {
        return output_appendf(out,
                              "//   %s s = config.%s;\n"
                              "//   const char* data = config.%s.data();\n",
                              field->type, field->name, field->name);
    }

    if (strcmp(field->type, "bool") == 0) {
        return output_appendf(out,
                              "//   if (config.%s) {\n"
                              "//   }\n",
                              field->name);
    }

    return output_appendf(out, "//   %s value = config.%s;\n", field->type, field->name);
}

static bool emit_usage_comment_for_field(cxgn_output* out, const cxgn_struct_parser* parser,
                                         const cxgn_field_info* field) {
    char* elem_type = extract_template_payload(field->type, "Array");
    if (elem_type) {
        const bool ok = output_appendf(
            out,
            "//   const %s* data = config.%s.data;\n"
            "//   size_t count = config.%s.size;\n",
            elem_type, field->name, field->name);
        free(elem_type);
        return ok;
    }

    char* value_type = extract_template_payload(field->type, "Optional");
    if (value_type) {
        const bool ok = output_appendf(
            out,
            "//   if (config.%s) {\n"
            "//       %s value = config.%s.value;\n"
            "//   }\n",
            field->name, value_type, field->name);
        free(value_type);
        return ok;
    }

    char** types = NULL;
    size_t type_count = 0;
    if (extract_variant_params(field->type, "std::variant", &types, &type_count)) {

        bool ok = output_appendf(out, "//   // %s alternatives:\n", field->name);
        for (size_t i = 0; ok && i < type_count; i++) {
            const char* alt = types[i];
            const cxgn_field_info* nested_first = first_field_of_struct(parser, alt);
            if (nested_first) {
                ok = output_appendf(
                    out,
                    "//   if (const %s* value = std::get_if<%s>(&config.%s)) {\n"
                    "//       (void)value->%s;\n"
                    "//   }\n",
                    alt, alt, field->name, nested_first->name);
            } else {
                ok = output_appendf(
                    out,
                    "//   if (const %s* value = std::get_if<%s>(&config.%s)) {\n"
                    "//       (void)*value;\n"
                    "//   }\n",
                    alt, alt, field->name);
            }
        }

        for (size_t i = 0; i < type_count; i++) free(types[i]);
        free(types);
        return ok;
    }

    const cxgn_field_info* nested_first = first_field_of_struct(parser, field->type);
    if (nested_first) {
        return output_appendf(out,
                              "//   const %s& value = config.%s;\n"
                              "//   (void)value.%s;\n",
                              field->type, field->name, nested_first->name);
    }

    return emit_scalar_usage_comment(out, field);
}

static bool emit_usage_comments(cxgn_output* out, const cxgn_struct_parser* parser,
                                const cxgn_struct_info* root_struct) {
    if (!out || !root_struct) return false;

    if (!output_append(out,
                       "// Usage examples:\n"
                       "//   Access fields directly on config.\n")) {
        return false;
    }

    for (size_t i = 0; i < root_struct->field_count; i++) {
        if (!emit_usage_comment_for_field(out, parser, &root_struct->fields[i])) {
            return false;
        }
    }

    return output_append(out, "\n");
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
    return cxgn_strndup(start, (size_t)(end - start));
}

static bool extract_variant_params(const char* type, const char* wrapper,
                                   char*** out_types, size_t* out_count) {
    if (!out_types || !out_count) return false;
    *out_types = NULL;
    *out_count = 0;

    char* payload = extract_template_payload(type, wrapper);
    if (!payload) return false;

    /* Count top-level commas to pre-allocate */
    size_t count = 1;
    int depth = 0;
    for (const char* p = payload; *p; ++p) {
        if (*p == '<') depth++;
        else if (*p == '>') depth--;
        else if (*p == ',' && depth == 0) count++;
    }

    char** types = (char**)malloc(count * sizeof(char*));
    if (!types) { free(payload); return false; }
    for (size_t i = 0; i < count; i++) types[i] = NULL;

    /* Split on top-level commas and trim each segment */
    size_t idx = 0;
    const char* seg = payload;
    depth = 0;
    for (const char* p = payload; ; ++p) {
        if (*p == '<') depth++;
        else if (*p == '>') depth--;

        bool at_sep = ((*p == ',' || *p == '\0') && depth == 0);
        if (at_sep) {
            const char* s = seg;
            const char* e = p;
            while (s < e && isspace((unsigned char)*s)) s++;
            while (e > s && isspace((unsigned char)*(e - 1))) e--;
            if (s >= e) goto fail;
            types[idx] = cxgn_strndup(s, (size_t)(e - s));
            if (!types[idx]) goto fail;
            idx++;
            seg = p + 1;
        }
        if (*p == '\0') break;
    }

    free(payload);
    *out_types = types;
    *out_count = count;
    return true;

fail:
    for (size_t i = 0; i < count; i++) free(types[i]);
    free(types);
    free(payload);
    return false;
}

static void reset_field_derived_types(cxgn_field_info* field) {
    if (!field) return;
    field->is_array = false;
    field->is_optional = false;
    field->is_variant = false;
    free(field->array_elem_type);
    free(field->optional_value_type);
    for (size_t i = 0; i < field->variant_type_count; i++)
        free(field->variant_types[i]);
    free(field->variant_types);
    field->array_elem_type = NULL;
    field->optional_value_type = NULL;
    field->variant_types = NULL;
    field->variant_type_count = 0;
}

static bool populate_field_type_traits(const cxgn_generator* gen, cxgn_field_info* field) {
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

    if (extract_variant_params(
            field->type, gen->variant_wrapper, &field->variant_types, &field->variant_type_count)) {
        field->is_variant = true;
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

/* ═══════════════════════════════════════════════════════════════════════════
 * Backing Name, String Pool, and Array Cache Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static char* escape_string(const char* value) {
    size_t n = 0;
    for (const char* p = value; *p; p++) {
        n += (*p == '"' || *p == '\\' || *p == '\n' || *p == '\t') ? 2 : 1;
    }
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    char* w = out;
    for (const char* p = value; *p; p++) {
        if      (*p == '"')  { *w++ = '\\'; *w++ = '"';  }
        else if (*p == '\\') { *w++ = '\\'; *w++ = '\\'; }
        else if (*p == '\n') { *w++ = '\\'; *w++ = 'n';  }
        else if (*p == '\t') { *w++ = '\\'; *w++ = 't';  }
        else { *w++ = *p; }
    }
    *w = '\0';
    return out;
}

static int type_sort_rank(const char* type) {
    if (!type) return 8;

    if (strcmp(type, "long double") == 0) return 16;

    if (strcmp(type, "double") == 0 ||
        strcmp(type, "int64_t") == 0 ||
        strcmp(type, "uint64_t") == 0 ||
        strcmp(type, "long long") == 0 ||
        strcmp(type, "unsigned long long") == 0 ||
        strcmp(type, "size_t") == 0 ||
        strcmp(type, "std::string") == 0 ||
        strcmp(type, "std::string_view") == 0) {
        return 8;
    }

    if (strcmp(type, "int") == 0 ||
        strcmp(type, "unsigned int") == 0 ||
        strcmp(type, "long") == 0 ||
        strcmp(type, "unsigned long") == 0 ||
        strcmp(type, "float") == 0 ||
        strcmp(type, "int32_t") == 0 ||
        strcmp(type, "uint32_t") == 0) {
        return 4;
    }

    if (strcmp(type, "short") == 0 ||
        strcmp(type, "unsigned short") == 0 ||
        strcmp(type, "int16_t") == 0 ||
        strcmp(type, "uint16_t") == 0) {
        return 2;
    }

    if (strcmp(type, "char") == 0 ||
        strcmp(type, "signed char") == 0 ||
        strcmp(type, "unsigned char") == 0 ||
        strcmp(type, "bool") == 0 ||
        strcmp(type, "int8_t") == 0 ||
        strcmp(type, "uint8_t") == 0) {
        return 1;
    }

    return 8;
}

static bool register_backing_decl(gen_context* ctx, backing_decl_kind kind,
                                  int sort_rank, const char* name,
                                  const char* code) {
    if (ctx->decl_count >= CXGN_MAX_BACKING_DECL) return false;

    backing_decl* decl = &ctx->decls[ctx->decl_count];
    decl->kind = kind;
    decl->sort_rank = sort_rank;
    decl->name = cxgn_strdup(name);
    decl->code = cxgn_strdup(code);
    if (!decl->name || !decl->code) {
        free(decl->name);
        free(decl->code);
        decl->name = NULL;
        decl->code = NULL;
        return false;
    }

    ctx->decl_count++;
    return true;
}

static void propagate_backing_decl_sort_ranks(gen_context* ctx) {
    int max_array_rank = 0;

    for (int i = 0; i < ctx->decl_count; i++) {
        const backing_decl* decl = &ctx->decls[i];
        if (decl->kind == BACKING_DECL_ARRAY && decl->sort_rank > max_array_rank) {
            max_array_rank = decl->sort_rank;
        }
    }

    for (int i = 0; i < ctx->decl_count; i++) {
        backing_decl* target = &ctx->decls[i];
        if (target->kind == BACKING_DECL_SPOOL && max_array_rank > target->sort_rank) {
            target->sort_rank = max_array_rank;
        }
    }
}

static int backing_decl_compare(const void* lhs, const void* rhs) {
    const backing_decl* a = (const backing_decl*)lhs;
    const backing_decl* b = (const backing_decl*)rhs;

    if (a->sort_rank != b->sort_rank) return b->sort_rank - a->sort_rank;
    if (a->kind != b->kind) return (int)a->kind - (int)b->kind;

    const int by_name = strcmp(a->name, b->name);
    if (by_name != 0) return by_name;

    return strcmp(a->code, b->code);
}

static bool flush_backing_decls(gen_context* ctx) {
    if (ctx->decl_count == 0) return true;

    propagate_backing_decl_sort_ranks(ctx);
    qsort(ctx->decls, (size_t)ctx->decl_count, sizeof(ctx->decls[0]),
          backing_decl_compare);

    for (int i = 0; i < ctx->decl_count; i++) {
        if (!output_append(ctx->backing, ctx->decls[i].code)) return false;
    }

    return true;
}

/* Intern a string value in the pool.  Emits a pool declaration to ctx->backing
 * on first use.  Returns the pool variable name, or NULL on failure (caller
 * falls back to inline literal). */
static const char* spool_intern(gen_context* ctx, const char* raw_value) {
    int i;
    for (i = 0; i < ctx->spool_count; i++) {
        if (strcmp(ctx->spool_raw[i], raw_value) == 0)
            return ctx->spool_name[i];
    }
    if (ctx->spool_count >= CXGN_MAX_SPOOL) return NULL;

    char* escaped = escape_string(raw_value);
    if (!escaped) return NULL;

    i = ctx->spool_count;
    char name_buf[32];
    snprintf(name_buf, sizeof(name_buf), "_spool_%d", i);

    ctx->spool_raw[i]     = cxgn_strdup(raw_value);
    ctx->spool_escaped[i] = escaped;
    ctx->spool_name[i]    = cxgn_strdup(name_buf);

    if (!ctx->spool_raw[i] || !ctx->spool_name[i]) {
        free(ctx->spool_raw[i]);
        free(ctx->spool_escaped[i]);
        free(ctx->spool_name[i]);
        ctx->spool_raw[i] = ctx->spool_escaped[i] = ctx->spool_name[i] = NULL;
        return NULL;
    }

    cxgn_output* decl = output_new();
    if (!decl) return NULL;

    if (!output_appendf(decl, "static constexpr char %s[] = \"%s\";\n",
                        name_buf, escaped) ||
        !register_backing_decl(ctx, BACKING_DECL_SPOOL, type_sort_rank("char"),
                               name_buf, decl->code)) {
        cxgn_output_free(decl);
        return NULL;
    }

    cxgn_output_free(decl);
    ctx->spool_count++;
    return ctx->spool_name[i];
}

static const char* arr_cache_find(const gen_context* ctx, const char* elem_type,
                                  const char* content) {
    int i;
    for (i = 0; i < ctx->arr_count; i++) {
        if (strcmp(ctx->arr_elem_type[i], elem_type) == 0 &&
            strcmp(ctx->arr_content[i], content) == 0)
            return ctx->arr_name[i];
    }
    return NULL;
}

static void arr_cache_store(gen_context* ctx, const char* elem_type,
                            const char* content, const char* name) {
    if (ctx->arr_count >= CXGN_MAX_ARR_CACHE) return;
    int i = ctx->arr_count;
    ctx->arr_elem_type[i] = cxgn_strdup(elem_type);
    ctx->arr_content[i]   = cxgn_strdup(content);
    ctx->arr_name[i]      = cxgn_strdup(name);
    if (!ctx->arr_elem_type[i] || !ctx->arr_content[i] || !ctx->arr_name[i]) {
        free(ctx->arr_elem_type[i]);
        free(ctx->arr_content[i]);
        free(ctx->arr_name[i]);
        ctx->arr_elem_type[i] = ctx->arr_content[i] = ctx->arr_name[i] = NULL;
        return;
    }
    ctx->arr_count++;
}

static bool is_name_used(const gen_context* ctx, const char* name) {
    int i;
    for (i = 0; i < ctx->used_count; i++) {
        if (strcmp(ctx->used_names[i], name) == 0) return true;
    }
    return false;
}

static void mark_name_used(gen_context* ctx, const char* name) {
    if (ctx->used_count >= CXGN_MAX_USED_NAMES) return;
    ctx->used_names[ctx->used_count] = cxgn_strdup(name);
    if (ctx->used_names[ctx->used_count]) ctx->used_count++;
}

/* Build a backing variable name derived from the current YAML path.
 * E.g. path "AllTypesConfig.intArray" → "_backing_AllTypesConfig_intArray". */
static char* make_path_backing_name(gen_context* ctx) {
    char* path_str = cxgn_path_to_string(ctx->path);
    if (!path_str) return NULL;

    /* Sanitise: '.' and '[' → '_',  ']' → nothing */
    char* rd = path_str;
    char* wr = path_str;
    while (*rd) {
        char c = *rd++;
        if      (c == '.' || c == '[') *wr++ = '_';
        else if (c == ']')             { /* skip */ }
        else                           *wr++ = c;
    }
    *wr = '\0';

    if (*path_str == '\0') {
        free(path_str);
        char* fb = (char*)malloc(32);
        if (fb) snprintf(fb, 32, "_backing_%d", ctx->backing_counter++);
        return fb;
    }

    const char* prefix = "_backing_";
    size_t plen = strlen(prefix);
    size_t slen = strlen(path_str);
    char* result = (char*)malloc(plen + slen + 1);
    if (result) {
        memcpy(result, prefix, plen);
        memcpy(result + plen, path_str, slen + 1);
    }
    free(path_str);
    return result;
}

/* Return a unique, path-derived backing name (handles collisions with a counter suffix). */
static char* resolve_backing_name(gen_context* ctx) {
    char* base = make_path_backing_name(ctx);
    if (!base) return NULL;

    if (!is_name_used(ctx, base)) {
        mark_name_used(ctx, base);
        return base;
    }

    size_t baselen = strlen(base);
    char* unique = (char*)malloc(baselen + 16);
    if (!unique) { free(base); return NULL; }
    snprintf(unique, baselen + 16, "%s_%d", base, ctx->backing_counter++);
    free(base);
    mark_name_used(ctx, unique);
    return unique;
}

static void gen_context_cleanup(gen_context* ctx) {
    int i;
    for (i = 0; i < ctx->spool_count; i++) {
        free(ctx->spool_raw[i]);
        free(ctx->spool_escaped[i]);
        free(ctx->spool_name[i]);
    }
    for (i = 0; i < ctx->arr_count; i++) {
        free(ctx->arr_elem_type[i]);
        free(ctx->arr_content[i]);
        free(ctx->arr_name[i]);
    }
    for (i = 0; i < ctx->used_count; i++) {
        free(ctx->used_names[i]);
    }
    for (i = 0; i < ctx->decl_count; i++) {
        free(ctx->decls[i].name);
        free(ctx->decls[i].code);
    }
}

/**
 * @brief Generate scalar value.
 */
static bool gen_scalar(gen_context* ctx, const char* value, const cxgn_field_info* field) {
    const char* type = field ? field->type : "auto";
    yaml_value_type vtype = detect_yaml_type(value);

    /* Check for expression field */
    if (ctx->gen->has_expr_handler && field &&
        ctx->gen->expr_handler.is_expression_field(type, ctx->gen->expr_handler.userdata)) {
        char* path_str = cxgn_path_to_string(ctx->path);
        char* code = ctx->gen->expr_handler.generate_code(value, path_str, ctx->gen->expr_handler.userdata);
        free(path_str);

        if (!code) {
            cxgn_error_set(ctx->err, CXGN_ERR_EXPRESSION_ERROR, "Expression generation failed");
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
        /* Use string pool to deduplicate identical literals */
        const char* pool_name = spool_intern(ctx, value);
        if (pool_name) {
            output_append(ctx->out, pool_name);
        } else {
            /* Pool full or OOM – fall back to inline literal */
            output_append(ctx->out, "\"");
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
        }
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
 *
 * Features:
 *  - Path-based backing name (e.g. _backing_AllTypesConfig_intArray)
 *  - Content-based deduplication: identical arrays share a single _data[] entry
 */
static bool gen_array(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                      const cxgn_field_info* field) {
    if (node->type != YAML_SEQUENCE_NODE) {
        cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Expected array");
        return false;
    }

    const char* elem_type = field->array_elem_type;

    /* Generate element content into a temporary buffer for deduplication */
    cxgn_output* temp = output_new();
    if (!temp) {
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    bool first = true;
    size_t index = 0;
    yaml_node_item_t* item;

    for (item = node->data.sequence.items.start;
         item < node->data.sequence.items.top; item++) {
        yaml_node_t* elem = yaml_document_get_node(doc, *item);
        if (!first) output_append(temp, ", ");
        first = false;

        cxgn_path_push_index(ctx->path, index);

        cxgn_field_info elem_field = {0};
        elem_field.type = (char*)elem_type;
        if (!populate_field_type_traits(ctx->gen, &elem_field)) {
            cxgn_output_free(temp);
            cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }

        /* Redirect ctx->out to temp; ctx->backing stays for pool / nested arrays */
        cxgn_output* saved_out = ctx->out;
        ctx->out = temp;
        bool ok = gen_value(ctx, doc, elem, &elem_field);
        ctx->out = saved_out;

        cxgn_path_pop(ctx->path);
        reset_field_derived_types(&elem_field);

        if (!ok) {
            cxgn_output_free(temp);
            return false;
        }
        index++;
    }

    char count_str[32];
    snprintf(count_str, sizeof(count_str), "%zu", index);

    /* Check for an identical array already in the cache */
    const char* cached_name = arr_cache_find(ctx, elem_type, temp->code);
    if (cached_name) {
        cxgn_output_free(temp);
        output_appendf(ctx->out, ctx->gen->array_ctor_fmt, elem_type, cached_name, count_str);
        return true;
    }

    /* Generate a path-based, collision-safe backing name */
    char* backing_name = resolve_backing_name(ctx);
    if (!backing_name) {
        cxgn_output_free(temp);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    /* Cache and emit backing array.
     * std::string is not a literal type before C++20: use 'const' there.
     * AUTO mode emits a #if __cplusplus guard so the file works with any standard. */
    arr_cache_store(ctx, elem_type, temp->code, backing_name);
    const bool is_non_literal = (strcmp(elem_type, "std::string") == 0);
    const cxgn_cpp_std std = ctx->gen->cpp_std;

    cxgn_output* decl = output_new();
    if (!decl) {
        cxgn_output_free(temp);
        free(backing_name);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    if (is_non_literal && std == CXGN_CPP_STD_AUTO) {
        if (!output_appendf(decl,
            "#if __cplusplus >= 202002L\n"
            "static constexpr %s %s_data[] = {%s};\n"
            "#else\n"
            "static const %s %s_data[] = {%s};\n"
            "#endif\n",
            elem_type, backing_name, temp->code,
            elem_type, backing_name, temp->code)) {
            cxgn_output_free(decl);
            cxgn_output_free(temp);
            free(backing_name);
            cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }
        ctx->has_non_constexpr_backing = true;
    } else {
        const bool use_constexpr = !is_non_literal || std >= CXGN_CPP_STD_20;
        if (!use_constexpr) ctx->has_non_constexpr_backing = true;
        if (!output_appendf(decl, "static %s %s %s_data[] = {%s};\n",
                            use_constexpr ? "constexpr" : "const",
                            elem_type, backing_name, temp->code)) {
            cxgn_output_free(decl);
            cxgn_output_free(temp);
            free(backing_name);
            cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }
    }

    if (!register_backing_decl(ctx, BACKING_DECL_ARRAY, type_sort_rank(elem_type),
                               backing_name, decl->code)) {
        cxgn_output_free(decl);
        cxgn_output_free(temp);
        free(backing_name);
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    cxgn_output_free(decl);
    cxgn_output_free(temp);

    output_appendf(ctx->out, ctx->gen->array_ctor_fmt, elem_type, backing_name, count_str);
    free(backing_name);
    return true;
}

/**
 * @brief Generate optional value.
 */
static bool gen_optional(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                         const cxgn_field_info* field) {
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

    cxgn_field_info inner_field = {0};
    inner_field.type = (char*)value_type;
    if (!populate_field_type_traits(ctx->gen, &inner_field)) {
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    bool ok = gen_value(ctx, doc, node, &inner_field);
    reset_field_derived_types(&inner_field);
    output_append(ctx->out, ctx->gen->optional_value_suffix);
    return ok;
}

static bool is_struct_mapping_type(gen_context* ctx, const char* type) {
    return type && cxgn_struct_parser_find_struct(ctx->gen->parser, type) != NULL;
}

static bool gen_variant(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                      const cxgn_field_info* field) {
    if (!field->variant_types || field->variant_type_count == 0) {
        cxgn_error_set(ctx->err, CXGN_ERR_PARSE_ERROR, "Invalid std::variant type");
        return false;
    }

    /* Pick the first type that matches the YAML node shape:
     * - mapping node → first struct type
     * - scalar node  → first non-struct type
     * Defaults to index 0 if no better match is found. */
    size_t chosen = 0;
    if (node->type == YAML_MAPPING_NODE) {
        for (size_t i = 0; i < field->variant_type_count; i++) {
            if (is_struct_mapping_type(ctx, field->variant_types[i])) {
                chosen = i;
                break;
            }
        }
    } else if (node->type == YAML_SCALAR_NODE) {
        for (size_t i = 0; i < field->variant_type_count; i++) {
            if (!is_struct_mapping_type(ctx, field->variant_types[i])) {
                chosen = i;
                break;
            }
        }
    }

    cxgn_field_info inner = {0};
    inner.type = field->variant_types[chosen];
    if (!populate_field_type_traits(ctx->gen, &inner)) {
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    const bool inner_is_struct = is_struct_mapping_type(ctx, inner.type);
    output_appendf(ctx->out, "%s{std::in_place_index<%zu>, ", field->type, chosen);
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
                      const cxgn_field_info* field) {
    if (!node || !field) return false;

    cxgn_field_info derived = {0};
    derived.type = field->type;
    if (!populate_field_type_traits(ctx->gen, &derived)) {
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
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

    /* Handle std::variant */
    if (derived.is_variant) {
        const bool ok = gen_variant(ctx, doc, node, &derived);
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
        const cxgn_struct_info* nested = cxgn_struct_parser_find_struct(ctx->gen->parser, field->type);
        if (!nested) {
            reset_field_derived_types(&derived);
            cxgn_error_set(ctx->err, CXGN_ERR_UNKNOWN_STRUCT, "Unknown struct type");
            return false;
        }
        const bool ok = gen_struct_value(ctx, doc, node, nested);
        reset_field_derived_types(&derived);
        return ok;
    }

    reset_field_derived_types(&derived);
    cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Unexpected YAML node type");
    return false;
}

/**
 * @brief Generate struct initialization.
 */
static bool gen_struct_value(gen_context* ctx, yaml_document_t* doc, yaml_node_t* node,
                             const cxgn_struct_info* info) {
    if (node->type != YAML_MAPPING_NODE) {
        cxgn_error_set(ctx->err, CXGN_ERR_TYPE_MISMATCH, "Expected mapping for struct");
        return false;
    }

    output_append(ctx->out, "{\n");
    ctx->indent++;

    const size_t pair_count =
        (size_t)(node->data.mapping.pairs.top - node->data.mapping.pairs.start);
    bool* pair_used = (bool*)calloc(pair_count, sizeof(bool));
    if (!pair_used) {
        cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    /* Process each field in struct order */
    for (size_t i = 0; i < info->field_count; i++) {
        const cxgn_field_info* field = &info->fields[i];

        /* Find corresponding YAML key */
        char* snake_name = cxgn_to_snake_case(ctx->gen->utils, field->name);
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
            cxgn_path_push(ctx->path, field->name);
            bool ok = gen_value(ctx, doc, value_node, field);
            cxgn_path_pop(ctx->path);
            if (!ok) {
                free(pair_used);
                return false;
            }
        } else if (field->default_value) {
            /* Use default value */
            warn_missing_header_field(ctx, info, field, "Using default value");
            output_append(ctx->out, field->default_value);
        } else {
            cxgn_field_info derived = {0};
            derived.type = field->type;
            const bool have_traits = populate_field_type_traits(ctx->gen, &derived);
            const bool is_optional = have_traits && derived.is_optional;
            char* optional_type = is_optional ? cxgn_strdup(derived.optional_value_type) : NULL;
            reset_field_derived_types(&derived);

            if (!have_traits) {
                free(pair_used);
                cxgn_error_set(ctx->err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
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

cxgn_generator* cxgn_generator_new(const cxgn_struct_parser* parser, const cxgn_string_utils* utils) {
    cxgn_generator* gen = (cxgn_generator*)calloc(1, sizeof(cxgn_generator));
    if (!gen) return NULL;

    gen->parser = parser;
    gen->utils = utils;
    gen->has_expr_handler = false;
    gen->array_wrapper = cxgn_strdup("Array");
    gen->optional_wrapper = cxgn_strdup("Optional");
    gen->variant_wrapper = cxgn_strdup("std::variant");
    gen->array_ctor_fmt = cxgn_strdup("Array<%s>{%s_data, %s}");
    gen->optional_empty_fmt = cxgn_strdup("Optional<%s>::empty()");
    gen->optional_value_prefix_fmt = cxgn_strdup("Optional<%s>{");
    gen->optional_value_suffix = cxgn_strdup("}");
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
    if (!gen) return;
    gen->cpp_std = std;
}

void cxgn_generator_set_expression_handler(cxgn_generator* gen, const cxgn_expression_handler* handler) {
    if (!gen || !handler) return;
    gen->expr_handler = *handler;
    gen->has_expr_handler = true;
}

void cxgn_generator_set_type_options(cxgn_generator* gen, const cxgn_type_options* options) {
    if (!gen || !options) return;

    char* next_array_wrapper = options->array_wrapper ? cxgn_strdup(options->array_wrapper) : NULL;
    char* next_optional_wrapper = options->optional_wrapper ? cxgn_strdup(options->optional_wrapper) : NULL;
    char* next_variant_wrapper = options->variant_wrapper ? cxgn_strdup(options->variant_wrapper) : NULL;
    char* next_array_ctor_fmt = options->array_ctor_fmt ? cxgn_strdup(options->array_ctor_fmt) : NULL;
    char* next_optional_empty_fmt = options->optional_empty_fmt ? cxgn_strdup(options->optional_empty_fmt) : NULL;
    char* next_optional_value_prefix_fmt =
        options->optional_value_prefix_fmt ? cxgn_strdup(options->optional_value_prefix_fmt) : NULL;
    char* next_optional_value_suffix =
        options->optional_value_suffix ? cxgn_strdup(options->optional_value_suffix) : NULL;

    if ((options->array_wrapper && !next_array_wrapper) ||
        (options->optional_wrapper && !next_optional_wrapper) ||
        (options->variant_wrapper && !next_variant_wrapper) ||
        (options->array_ctor_fmt && !next_array_ctor_fmt) ||
        (options->optional_empty_fmt && !next_optional_empty_fmt) ||
        (options->optional_value_prefix_fmt && !next_optional_value_prefix_fmt) ||
        (options->optional_value_suffix && !next_optional_value_suffix)) {
        free(next_array_wrapper);
        free(next_optional_wrapper);
        free(next_variant_wrapper);
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
    if (next_variant_wrapper) {
        free(gen->variant_wrapper);
        gen->variant_wrapper = next_variant_wrapper;
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

static cxgn_output* cxgn_generate_from_document(cxgn_generator* gen,
                                            yaml_document_t* doc,
                                            const char* yaml_path,
                                            const char* header_path,
                                            cxgn_error* err) {
    yaml_node_t* root = yaml_document_get_root_node(doc);
    if (!root || root->type != YAML_MAPPING_NODE) {
        cxgn_error_set(err, CXGN_ERR_YAML_ERROR, "YAML root must be a mapping");
        return NULL;
    }

    /* Create output buffers */
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
    const cxgn_struct_info* root_struct = NULL;
    size_t struct_count = cxgn_struct_parser_get_struct_count(gen->parser);
    if (struct_count > 0) {
        root_struct = cxgn_struct_parser_get_struct(gen->parser, struct_count - 1);
    }

    if (!root_struct) {
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_path_free(path);
        cxgn_error_set(err, CXGN_ERR_UNKNOWN_STRUCT, "No struct found in header");
        return NULL;
    }

    /* Generate config initialization into a temporary buffer so we can choose
     * the correct qualifier ('constexpr' vs 'const') after generation. */
    cxgn_output* body = output_new();
    if (!body) {
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_path_free(path);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    cxgn_output* saved_out = ctx.out;
    ctx.out = body;

    cxgn_path_push(path, root_struct->name);
    bool ok = gen_struct_value(&ctx, doc, root, root_struct);
    cxgn_path_pop(path);

    ctx.out = saved_out;
    cxgn_path_free(path);

    if (!emit_usage_comments(out, gen->parser, root_struct)) {
        cxgn_output_free(body);
        gen_context_cleanup(&ctx);
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    /* Emit config variable with correct qualifier.
     * AUTO mode uses a #if __cplusplus guard when non-literal backing was needed. */
    if (gen->cpp_std == CXGN_CPP_STD_AUTO && ctx.has_non_constexpr_backing) {
        output_appendf(out, "#if __cplusplus >= 202002L\n"
                            "constexpr %s config = ", root_struct->name);
        output_append(out, body->code);
        output_append(out, ";\n#else\nconst ");
        output_appendf(out, "%s config = ", root_struct->name);
        output_append(out, body->code);
        output_append(out, ";\n#endif\n");
    } else {
        const char* qualifier =
            (gen->cpp_std >= CXGN_CPP_STD_20 || !ctx.has_non_constexpr_backing)
            ? "constexpr" : "const";
        output_appendf(out, "%s %s config = ", qualifier, root_struct->name);
        output_append(out, body->code);
        output_append(out, ";\n");
    }
    cxgn_output_free(body);

    if (!ok) {
        gen_context_cleanup(&ctx);
        cxgn_output_free(out);
        cxgn_output_free(backing);
        return NULL;
    }

    if (!flush_backing_decls(&ctx)) {
        gen_context_cleanup(&ctx);
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    /* Combine backing storage and main output */
    output_append(backing, "} // namespace\n\n");

    cxgn_output* final = output_new();
    if (!final) {
        gen_context_cleanup(&ctx);
        cxgn_output_free(out);
        cxgn_output_free(backing);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    output_append(final, backing->code);
    output_append(final, out->code);

    gen_context_cleanup(&ctx);
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
        cxgn_error_set(err, CXGN_ERR_YAML_ERROR, "Failed to parse YAML");
        yaml_parser_delete(&parser);
        fclose(f);
        return NULL;
    }

    cxgn_output* result = cxgn_generate_from_document(gen, &doc, yaml_path, header_path, err);

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    fclose(f);
    return result;
}

cxgn_output* cxgn_generate_from_yaml_text(cxgn_generator* gen,
                                      const char* yaml_text,
                                      const char* yaml_virtual_path,
                                      const char* header_path,
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
    yaml_parser_set_input_string(
        &parser, (const unsigned char*)yaml_text, strlen(yaml_text));

    if (!yaml_parser_load(&parser, &doc)) {
        cxgn_error_set(err, CXGN_ERR_YAML_ERROR, "Failed to parse YAML");
        yaml_parser_delete(&parser);
        return NULL;
    }

    const char* path_for_diag =
        (yaml_virtual_path && yaml_virtual_path[0] != '\0') ? yaml_virtual_path : "<in-memory-yaml>";
    cxgn_output* result = cxgn_generate_from_document(gen, &doc, path_for_diag, header_path, err);

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    return result;
}

const char* cxgn_output_get_code(const cxgn_output* output) {
    return output ? output->code : NULL;
}

size_t cxgn_output_get_code_length(const cxgn_output* output) {
    return output ? output->length : 0;
}

void cxgn_output_free(cxgn_output* output) {
    if (!output) return;
    free(output->code);
    free(output);
}
