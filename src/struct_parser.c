/**
 * @file struct_parser.c
 * @brief Lightweight C header parser for cxgn schemas.
 */

#include "internal.h"
#include <ctype.h>
#include <stdio.h>

static const char* const builtin_types[] = {
    "int", "unsigned int", "long", "unsigned long", "long long",
    "unsigned long long", "short", "unsigned short", "char", "unsigned char",
    "signed char", "float", "double", "long double", "bool", "size_t",
    "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
    "uint32_t", "uint64_t", "const char*", "std::string_view", "string_view", NULL
};

static const char* cxgn_skip_whitespace(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static bool cxgn_starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static size_t cxgn_extract_identifier(const char* s) {
    size_t len = 0;
    if (isalpha((unsigned char)s[0]) || s[0] == '_') {
        len = 1;
        while (isalnum((unsigned char)s[len]) || s[len] == '_') len++;
    }
    return len;
}

static void cxgn_trim_trailing_whitespace(char* s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static void cxgn_field_info_free(cxgn_field_info* field) {
    if (!field) return;
    free(field->name);
    free(field->type);
    free(field->default_value);
    free(field->array_elem_type);
    free(field->optional_value_type);
}

static void cxgn_struct_info_free(cxgn_struct_info* info) {
    if (!info) return;
    free(info->name);
    free(info->defined_in);
    for (size_t i = 0; i < info->field_count; i++) cxgn_field_info_free(&info->fields[i]);
    free(info->fields);
}

static void cxgn_alias_free(cxgn_type_alias* alias) {
    if (!alias) return;
    free(alias->name);
    free(alias->value_type);
}

static void cxgn_enum_info_free(struct cxgn_enum_info* info) {
    if (!info) return;
    free(info->name);
    free(info->defined_in);
    for (size_t i = 0; i < info->value_count; i++) free(info->values[i].name);
    free(info->values);
}

static cxgn_struct_info* cxgn_parser_add_struct(cxgn_struct_parser* parser, const char* name, const char* file) {
    if (parser->struct_count >= parser->struct_capacity) {
        size_t new_cap = parser->struct_capacity ? parser->struct_capacity * 2 : 8;
        cxgn_struct_info* next = (cxgn_struct_info*)realloc(parser->structs, new_cap * sizeof(*next));
        if (!next) return NULL;
        parser->structs = next;
        parser->struct_capacity = new_cap;
    }

    cxgn_struct_info* info = &parser->structs[parser->struct_count++];
    memset(info, 0, sizeof(*info));
    info->name = cxgn_strdup(name);
    info->defined_in = cxgn_strdup(file);
    info->field_capacity = 8;
    info->fields = (cxgn_field_info*)calloc(info->field_capacity, sizeof(*info->fields));
    if (!info->name || !info->defined_in || !info->fields) {
        cxgn_struct_info_free(info);
        memset(info, 0, sizeof(*info));
        parser->struct_count--;
        return NULL;
    }
    return info;
}

static cxgn_field_info* cxgn_struct_add_field(cxgn_struct_info* info) {
    if (info->field_count >= info->field_capacity) {
        size_t new_cap = info->field_capacity ? info->field_capacity * 2 : 8;
        cxgn_field_info* next = (cxgn_field_info*)realloc(info->fields, new_cap * sizeof(*next));
        if (!next) return NULL;
        info->fields = next;
        info->field_capacity = new_cap;
    }

    cxgn_field_info* field = &info->fields[info->field_count++];
    memset(field, 0, sizeof(*field));
    return field;
}

static bool cxgn_parser_was_file_parsed(cxgn_struct_parser* parser, const char* path) {
    for (size_t i = 0; i < parser->parsed_file_count; i++) {
        if (strcmp(parser->parsed_files[i], path) == 0) return true;
    }
    return false;
}

static bool cxgn_parser_mark_file_parsed(cxgn_struct_parser* parser, const char* path) {
    if (parser->parsed_file_count >= parser->parsed_file_capacity) {
        size_t new_cap = parser->parsed_file_capacity ? parser->parsed_file_capacity * 2 : 8;
        char** next = (char**)realloc(parser->parsed_files, new_cap * sizeof(*next));
        if (!next) return false;
        parser->parsed_files = next;
        parser->parsed_file_capacity = new_cap;
    }

    parser->parsed_files[parser->parsed_file_count] = cxgn_strdup(path);
    if (!parser->parsed_files[parser->parsed_file_count]) return false;
    parser->parsed_file_count++;
    return true;
}

const cxgn_type_alias* cxgn_struct_parser_find_alias(const cxgn_struct_parser* parser,
                                                     const char* name) {
    if (!parser || !name) return NULL;
    for (size_t i = 0; i < parser->alias_count; i++) {
        if (strcmp(parser->aliases[i].name, name) == 0) return &parser->aliases[i];
    }
    return NULL;
}

static bool cxgn_parser_add_alias(cxgn_struct_parser* parser, const char* name,
                             const char* value_type, cxgn_type_alias_kind kind) {
    const cxgn_type_alias* existing = cxgn_struct_parser_find_alias(parser, name);
    if (existing) return true;

    if (parser->alias_count >= parser->alias_capacity) {
        size_t new_cap = parser->alias_capacity ? parser->alias_capacity * 2 : 8;
        cxgn_type_alias* next = (cxgn_type_alias*)realloc(parser->aliases, new_cap * sizeof(*next));
        if (!next) return false;
        parser->aliases = next;
        parser->alias_capacity = new_cap;
    }

    cxgn_type_alias* alias = &parser->aliases[parser->alias_count++];
    memset(alias, 0, sizeof(*alias));
    alias->name = cxgn_strdup(name);
    alias->value_type = cxgn_strdup(value_type);
    alias->kind = kind;
    if (!alias->name || !alias->value_type) {
        cxgn_alias_free(alias);
        memset(alias, 0, sizeof(*alias));
        parser->alias_count--;
        return false;
    }
    return true;
}

static struct cxgn_enum_info* cxgn_parser_add_enum(cxgn_struct_parser* parser, const char* name, const char* file) {
    if (parser->enum_count >= parser->enum_capacity) {
        size_t new_cap = parser->enum_capacity ? parser->enum_capacity * 2 : 8;
        struct cxgn_enum_info* next = (struct cxgn_enum_info*)realloc(parser->enums, new_cap * sizeof(*next));
        if (!next) return NULL;
        parser->enums = next;
        parser->enum_capacity = new_cap;
    }

    struct cxgn_enum_info* info = &parser->enums[parser->enum_count++];
    memset(info, 0, sizeof(*info));
    info->name = cxgn_strdup(name);
    info->defined_in = cxgn_strdup(file);
    if (!info->name || !info->defined_in) {
        cxgn_enum_info_free(info);
        memset(info, 0, sizeof(*info));
        parser->enum_count--;
        return NULL;
    }
    return info;
}

static bool cxgn_enum_add_value(struct cxgn_enum_info* info, const char* name) {
    if (info->value_count >= info->value_capacity) {
        size_t new_cap = info->value_capacity ? info->value_capacity * 2 : 8;
        cxgn_enum_value_info* next =
            (cxgn_enum_value_info*)realloc(info->values, new_cap * sizeof(*next));
        if (!next) return false;
        info->values = next;
        info->value_capacity = new_cap;
    }

    cxgn_enum_value_info* value = &info->values[info->value_count++];
    memset(value, 0, sizeof(*value));
    value->name = cxgn_strdup(name);
    if (!value->name) {
        info->value_count--;
        return false;
    }
    return true;
}

static bool cxgn_parse_macro_alias(cxgn_struct_parser* parser, const char* line) {
    const char* kind = NULL;
    cxgn_type_alias_kind alias_kind = 0;
    if (cxgn_starts_with(line, "CXGN_ARRAY_TYPEDEF(")) {
        kind = "CXGN_ARRAY_TYPEDEF(";
        alias_kind = CXGN_ALIAS_ARRAY;
    } else if (cxgn_starts_with(line, "CXGN_OPTIONAL_TYPEDEF(")) {
        kind = "CXGN_OPTIONAL_TYPEDEF(";
        alias_kind = CXGN_ALIAS_OPTIONAL;
    } else {
        return false;
    }

    const char* p = line + strlen(kind);
    const char* comma = strchr(p, ',');
    const char* close = strchr(p, ')');
    if (!comma || !close || comma > close) return false;

    char* value_type = cxgn_strndup(p, (size_t)(comma - p));
    char* alias_name = cxgn_strndup(comma + 1, (size_t)(close - comma - 1));
    if (!value_type || !alias_name) {
        free(value_type);
        free(alias_name);
        return false;
    }

    cxgn_trim_trailing_whitespace(value_type);
    cxgn_trim_trailing_whitespace(alias_name);
    char* alias_trim = alias_name;
    while (*alias_trim && isspace((unsigned char)*alias_trim)) alias_trim++;
    const bool ok = cxgn_parser_add_alias(parser, alias_trim, cxgn_skip_whitespace(value_type), alias_kind);
    free(value_type);
    free(alias_name);
    return ok;
}

static bool cxgn_parse_typedef_alias(cxgn_struct_parser* parser, const char* line) {
    const char* p = cxgn_skip_whitespace(line);
    if (!cxgn_starts_with(p, "typedef struct {")) return false;

    const char* close = strchr(p, '}');
    const char* semi = close ? strchr(close, ';') : NULL;
    if (!close || !semi) return false;

    char* body = cxgn_strndup(p + strlen("typedef struct {"), (size_t)(close - (p + strlen("typedef struct {"))));
    char* alias = cxgn_strndup(close + 1, (size_t)(semi - close - 1));
    if (!body || !alias) {
        free(body);
        free(alias);
        return false;
    }
    cxgn_trim_trailing_whitespace(body);
    cxgn_trim_trailing_whitespace(alias);
    char* alias_trim = alias;
    while (*alias_trim && isspace((unsigned char)*alias_trim)) alias_trim++;

    bool ok = false;
    if (strstr(body, " size_t count;")) {
        const char* prefix = strstr(body, "const ");
        const char* star = prefix ? strstr(prefix, "* data;") : NULL;
        if (prefix && star && prefix < star) {
            char* value_type = cxgn_strndup(prefix, (size_t)(star - prefix));
            if (value_type) {
                cxgn_trim_trailing_whitespace(value_type);
                ok = cxgn_parser_add_alias(parser, alias_trim, value_type, CXGN_ALIAS_ARRAY);
                free(value_type);
            }
        }
    } else if (strstr(body, " bool has_value;")) {
        const char* value_end = strstr(body, " value;");
        if (value_end) {
            char* value_type = cxgn_strndup(body, (size_t)(value_end - body));
            if (value_type) {
                cxgn_trim_trailing_whitespace(value_type);
                ok = cxgn_parser_add_alias(parser, cxgn_skip_whitespace(alias_trim), cxgn_skip_whitespace(value_type),
                                      CXGN_ALIAS_OPTIONAL);
                free(value_type);
            }
        }
    }

    free(body);
    free(alias);
    return ok;
}

/**
 * Parses a simple scalar typedef such as:
 *   typedef const char* my_expr_t;
 *   typedef double price_t;
 * and registers the alias so the code generator can resolve the underlying type.
 * Does NOT handle "typedef struct" (handled by cxgn_parse_typedef_alias).
 */
static bool cxgn_parse_scalar_typedef(cxgn_struct_parser* parser, const char* line) {
    const char* p = cxgn_skip_whitespace(line);
    if (!cxgn_starts_with(p, "typedef ")) return false;
    p += strlen("typedef ");
    p = cxgn_skip_whitespace(p);
    if (cxgn_starts_with(p, "struct ")) return false; /* handled elsewhere */
    const char* semi = strchr(p, ';');
    if (!semi) return false;
    /* Find last identifier token before the semicolon — that's the alias name. */
    const char* name_end = semi;
    while (name_end > p && isspace((unsigned char)*(name_end - 1))) name_end--;
    const char* name_start = name_end;
    while (name_start > p && (isalnum((unsigned char)*(name_start - 1)) || *(name_start - 1) == '_'))
        name_start--;
    if (name_start >= name_end) return false;
    /* The underlying type is everything between "typedef " and the alias name. */
    const char* type_end = name_start;
    while (type_end > p && isspace((unsigned char)*(type_end - 1))) type_end--;
    if (type_end <= p) return false;
    char* value_type = cxgn_strndup(p, (size_t)(type_end - p));
    char* alias_name = cxgn_strndup(name_start, (size_t)(name_end - name_start));
    if (!value_type || !alias_name) {
        free(value_type);
        free(alias_name);
        return false;
    }
    const bool ok = cxgn_parser_add_alias(parser, alias_name, value_type, CXGN_ALIAS_SCALAR);
    free(value_type);
    free(alias_name);
    return ok;
}

static bool cxgn_parse_field_line(const cxgn_struct_parser* parser, const char* line, cxgn_field_info* field) {
    const char* p = cxgn_skip_whitespace(line);
    if (!*p || *p == '/' || *p == '#' || *p == '}') return false;
    if (strchr(p, '=') != NULL) return false;

    const char* semi = strchr(p, ';');
    if (!semi) return false;

    const char* name_end = semi;
    while (name_end > p && isspace((unsigned char)*(name_end - 1))) name_end--;
    const char* name_start = name_end;
    while (name_start > p && !isspace((unsigned char)*(name_start - 1))) name_start--;
    if (name_start == name_end) return false;

    field->name = cxgn_strndup(name_start, (size_t)(name_end - name_start));
    field->type = cxgn_strndup(p, (size_t)(name_start - p));
    if (!field->name || !field->type) {
        cxgn_field_info_free(field);
        memset(field, 0, sizeof(*field));
        return false;
    }
    cxgn_trim_trailing_whitespace(field->type);

    const cxgn_type_alias* alias = cxgn_struct_parser_find_alias(parser, field->type);
    if (alias) {
        if (alias->kind == CXGN_ALIAS_ARRAY) {
            field->is_array = true;
            field->array_elem_type = cxgn_strdup(alias->value_type);
        } else if (alias->kind == CXGN_ALIAS_OPTIONAL) {
            field->is_optional = true;
            field->optional_value_type = cxgn_strdup(alias->value_type);
        }
    }

    return true;
}

static bool cxgn_has_multi_decl_comma(const char* line) {
    int paren = 0;
    for (const char* p = line; *p && *p != ';'; p++) {
        if (*p == '(') paren++;
        else if (*p == ')') paren--;
        else if (*p == ',' && paren == 0) return true;
    }
    return false;
}

static bool cxgn_parse_multi_field_line(const cxgn_struct_parser* parser, const char* line,
                                   cxgn_struct_info* info) {
    const char* semi = strchr(line, ';');
    const char* comma = strchr(line, ',');
    if (!semi || !comma || comma > semi) return false;

    size_t prefix_len = (size_t)(comma - line);
    char* synth = (char*)malloc(prefix_len + 2);
    if (!synth) return false;
    memcpy(synth, line, prefix_len);
    synth[prefix_len] = ';';
    synth[prefix_len + 1] = '\0';

    cxgn_field_info first = {0};
    if (!cxgn_parse_field_line(parser, synth, &first)) {
        free(synth);
        return false;
    }
    cxgn_field_info* target = cxgn_struct_add_field(info);
    if (!target) {
        free(synth);
        cxgn_field_info_free(&first);
        return false;
    }
    *target = first;

    const char* type_str = first.type;
    const char* cur = comma + 1;
    while (cur < semi) {
        cur = cxgn_skip_whitespace(cur);
        const char* next = cur;
        while (next < semi && *next != ',') next++;
        const char* trimmed_end = next;
        while (trimmed_end > cur && isspace((unsigned char)*(trimmed_end - 1))) trimmed_end--;
        if (trimmed_end > cur) {
            size_t type_len = strlen(type_str);
            size_t name_len = (size_t)(trimmed_end - cur);
            char* entry = (char*)malloc(type_len + 1 + name_len + 2);
            if (!entry) {
                free(synth);
                return false;
            }
            memcpy(entry, type_str, type_len);
            entry[type_len] = ' ';
            memcpy(entry + type_len + 1, cur, name_len);
            entry[type_len + 1 + name_len] = ';';
            entry[type_len + 1 + name_len + 1] = '\0';
            cxgn_field_info extra = {0};
            if (cxgn_parse_field_line(parser, entry, &extra)) {
                cxgn_field_info* slot = cxgn_struct_add_field(info);
                if (slot) *slot = extra;
                else {
                    free(entry);
                    free(synth);
                    cxgn_field_info_free(&extra);
                    return false;
                }
            }
            free(entry);
        }
        cur = next + 1;
    }

    free(synth);
    return true;
}

static bool cxgn_register_struct_wrapper_alias(cxgn_struct_parser* parser,
                                          const cxgn_struct_info* info) {
    const cxgn_field_info* first;
    const cxgn_field_info* second;
    const char* pointee_end;
    size_t pointee_len;
    char* value_type;

    if (!parser || !info || !info->name || info->field_count != 2) return true;

    first = &info->fields[0];
    second = &info->fields[1];

    if (strcmp(first->name, "data") == 0 && strcmp(second->name, "count") == 0 &&
        second->type && strcmp(second->type, "size_t") == 0 &&
        first->type && strchr(first->type, '*') != NULL) {
        pointee_end = strrchr(first->type, '*');
        if (!pointee_end) return true;
        pointee_len = (size_t)(pointee_end - first->type);
        value_type = cxgn_strndup(first->type, pointee_len);
        if (!value_type) return false;
        cxgn_trim_trailing_whitespace(value_type);
        const bool ok = cxgn_parser_add_alias(parser, info->name, value_type, CXGN_ALIAS_ARRAY);
        free(value_type);
        return ok;
    }

    if (strcmp(first->name, "value") == 0 && strcmp(second->name, "has_value") == 0 &&
        second->type && strcmp(second->type, "bool") == 0) {
        return cxgn_parser_add_alias(parser, info->name, first->type, CXGN_ALIAS_OPTIONAL);
    }

    return true;
}

static bool cxgn_parse_struct(cxgn_struct_parser* parser, const char* content, const char* file,
                         size_t* pos, cxgn_error* err) {
    const char* p = content + *pos;
    bool has_typedef = false;
    if (cxgn_starts_with(p, "typedef")) {
        has_typedef = true;
        p = cxgn_skip_whitespace(p + strlen("typedef"));
    }
    if (!cxgn_starts_with(p, "struct")) return false;
    p = cxgn_skip_whitespace(p + strlen("struct"));

    char* name = NULL;
    size_t name_len = cxgn_extract_identifier(p);
    if (name_len > 0) {
        name = cxgn_strndup(p, name_len);
        p += name_len;
        p = cxgn_skip_whitespace(p);
    }

    if (*p != '{') {
        free(name);
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
        *pos = (size_t)(p - content);
        return true;
    }
    p++;

    const char* body_start = p;
    int brace_depth = 1;
    while (*p && brace_depth > 0) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') brace_depth--;
        p++;
    }
    if (brace_depth != 0) {
        free(name);
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Unterminated struct definition");
        return false;
    }

    const char* close_brace = p - 1;
    const char* after = cxgn_skip_whitespace(p);
    if (!name && has_typedef) {
        size_t alias_len = cxgn_extract_identifier(after);
        if (alias_len == 0) {
            cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Expected typedef struct name");
            return false;
        }
        name = cxgn_strndup(after, alias_len);
        after = cxgn_skip_whitespace(after + alias_len);
    }

    if (!name) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Expected struct name");
        return false;
    }

    if (*after != ';') {
        while (*after && *after != ';') after++;
    }
    if (*after == ';') after++;

    cxgn_struct_info* info = cxgn_parser_add_struct(parser, name, file);
    free(name);
    if (!info) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    const char* line = body_start;
    while (line < close_brace) {
        const char* line_end = memchr(line, '\n', (size_t)(close_brace - line));
        if (!line_end) line_end = close_brace;

        size_t len = (size_t)(line_end - line);
        char* buf = cxgn_strndup(line, len);
        if (!buf) {
            cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }

        if (cxgn_has_multi_decl_comma(buf)) {
            if (!cxgn_parse_multi_field_line(parser, buf, info)) {
                free(buf);
                cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
                return false;
            }
        } else {
            cxgn_field_info field = {0};
            if (cxgn_parse_field_line(parser, buf, &field)) {
                cxgn_field_info* slot = cxgn_struct_add_field(info);
                if (slot) *slot = field;
                else {
                    free(buf);
                    cxgn_field_info_free(&field);
                    cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
                    return false;
                }
            }
        }
        free(buf);

        line = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    if (has_typedef && !cxgn_register_struct_wrapper_alias(parser, info)) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    *pos = (size_t)(after - content);
    return true;
}

static bool cxgn_parse_enum(cxgn_struct_parser* parser, const char* content, const char* file,
                       size_t* pos, cxgn_error* err) {
    const char* p = content + *pos;
    bool has_typedef = false;
    char* tag_name = NULL;
    char* enum_name = NULL;
    struct cxgn_enum_info* info = NULL;

    if (cxgn_starts_with(p, "typedef")) {
        has_typedef = true;
        p = cxgn_skip_whitespace(p + strlen("typedef"));
    }
    if (!cxgn_starts_with(p, "enum")) return false;
    p = cxgn_skip_whitespace(p + strlen("enum"));

    size_t tag_len = cxgn_extract_identifier(p);
    if (tag_len > 0) {
        tag_name = cxgn_strndup(p, tag_len);
        p += tag_len;
        p = cxgn_skip_whitespace(p);
    }

    if (*p != '{') {
        free(tag_name);
        return false;
    }
    p++;

    const char* body_start = p;
    int brace_depth = 1;
    while (*p && brace_depth > 0) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') brace_depth--;
        p++;
    }
    if (brace_depth != 0) {
        free(tag_name);
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Unterminated enum definition");
        return false;
    }

    const char* close_brace = p - 1;
    const char* after = cxgn_skip_whitespace(p);
    if (has_typedef) {
        size_t alias_len = cxgn_extract_identifier(after);
        if (alias_len == 0) {
            free(tag_name);
            cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Expected typedef enum name");
            return false;
        }
        enum_name = cxgn_strndup(after, alias_len);
        after = cxgn_skip_whitespace(after + alias_len);
    } else if (tag_name) {
        enum_name = cxgn_strdup(tag_name);
    }

    free(tag_name);
    if (!enum_name) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Expected enum name");
        return false;
    }

    if (*after != ';') {
        while (*after && *after != ';') after++;
    }
    if (*after == ';') after++;

    info = cxgn_parser_add_enum(parser, enum_name, file);
    free(enum_name);
    if (!info) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    const char* cur = body_start;
    while (cur < close_brace) {
        while (cur < close_brace && (isspace((unsigned char)*cur) || *cur == ',')) cur++;
        if (cur >= close_brace) break;

        if (cur + 1 < close_brace && cur[0] == '/' && cur[1] == '/') {
            while (cur < close_brace && *cur != '\n') cur++;
            continue;
        }
        if (cur + 1 < close_brace && cur[0] == '/' && cur[1] == '*') {
            cur += 2;
            while (cur + 1 < close_brace && !(cur[0] == '*' && cur[1] == '/')) cur++;
            if (cur + 1 < close_brace) cur += 2;
            continue;
        }

        size_t name_len = cxgn_extract_identifier(cur);
        if (name_len == 0) {
            cur++;
            continue;
        }
        char* value_name = cxgn_strndup(cur, name_len);
        if (!value_name || !cxgn_enum_add_value(info, value_name)) {
            free(value_name);
            cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }
        free(value_name);
        cur += name_len;

        while (cur < close_brace && isspace((unsigned char)*cur)) cur++;
        if (cur < close_brace && *cur == '=') {
            cur++;
            while (cur < close_brace && *cur != ',') cur++;
        }
    }

    *pos = (size_t)(after - content);
    return true;
}

static bool cxgn_parse_include(cxgn_struct_parser* parser, const char* content, const char* base_dir,
                          size_t* pos, cxgn_error* err) {
    const char* p = cxgn_skip_whitespace(content + *pos + strlen("#include"));
    char delimiter = *p;
    if (delimiter != '"') {
        while (*p && *p != '\n') p++;
        *pos = (size_t)(p - content);
        return true;
    }

    p++;
    const char* end = strchr(p, '"');
    if (!end) {
        while (*p && *p != '\n') p++;
        *pos = (size_t)(p - content);
        return true;
    }

    char* rel = cxgn_strndup(p, (size_t)(end - p));
    char* full = rel ? cxgn_path_join(base_dir, rel) : NULL;
    free(rel);
    if (full && !cxgn_parser_was_file_parsed(parser, full)) cxgn_struct_parser_parse_file(parser, full, err);
    free(full);

    while (*end && *end != '\n') end++;
    *pos = (size_t)(end - content);
    return true;
}

static bool cxgn_strip_c_comments(char* content, cxgn_error* err) {
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;

    for (char* p = content; *p; p++) {
        if (in_string || in_char) {
            if (escaped) {
                escaped = false;
            } else if (*p == '\\') {
                escaped = true;
            } else if (in_string && *p == '"') {
                in_string = false;
            } else if (in_char && *p == '\'') {
                in_char = false;
            }
            continue;
        }

        if (*p == '"') {
            in_string = true;
            continue;
        }
        if (*p == '\'') {
            in_char = true;
            continue;
        }

        if (p[0] == '/' && p[1] == '/') {
            *p++ = ' ';
            *p = ' ';
            while (p[1] && p[1] != '\n') *++p = ' ';
            continue;
        }

        if (p[0] == '/' && p[1] == '*') {
            char* q = p + 2;
            p[0] = ' ';
            p[1] = ' ';
            while (q[0] && q[1] && !(q[0] == '*' && q[1] == '/')) {
                if (*q != '\n') *q = ' ';
                q++;
            }
            if (!q[0] || !q[1]) {
                cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Unterminated block comment");
                return false;
            }
            q[0] = ' ';
            q[1] = ' ';
            p = q + 1;
        }
    }

    return true;
}

cxgn_struct_parser* cxgn_struct_parser_new(const cxgn_string_utils* utils) {
    if (!utils) return NULL;
    cxgn_struct_parser* parser = (cxgn_struct_parser*)calloc(1, sizeof(*parser));
    if (!parser) return NULL;
    parser->ref_count = 1;
    parser->utils = cxgn_string_utils_retain((cxgn_string_utils*)utils);
    parser->struct_capacity = 8;
    parser->enum_capacity = 8;
    parser->parsed_file_capacity = 8;
    parser->alias_capacity = 8;
    parser->structs = (cxgn_struct_info*)calloc(parser->struct_capacity, sizeof(*parser->structs));
    parser->enums = (struct cxgn_enum_info*)calloc(parser->enum_capacity, sizeof(*parser->enums));
    parser->parsed_files = (char**)calloc(parser->parsed_file_capacity, sizeof(*parser->parsed_files));
    parser->aliases = (cxgn_type_alias*)calloc(parser->alias_capacity, sizeof(*parser->aliases));
    if (!parser->structs || !parser->enums || !parser->parsed_files || !parser->aliases) {
        cxgn_struct_parser_free(parser);
        return NULL;
    }
    return parser;
}

cxgn_struct_parser* cxgn_struct_parser_retain(cxgn_struct_parser* parser) {
    if (parser) parser->ref_count++;
    return parser;
}

void cxgn_struct_parser_free(cxgn_struct_parser* parser) {
    if (!parser) return;
    if (parser->ref_count > 1) {
        parser->ref_count--;
        return;
    }
    for (size_t i = 0; i < parser->struct_count; i++) cxgn_struct_info_free(&parser->structs[i]);
    for (size_t i = 0; i < parser->enum_count; i++) cxgn_enum_info_free(&parser->enums[i]);
    for (size_t i = 0; i < parser->parsed_file_count; i++) free(parser->parsed_files[i]);
    for (size_t i = 0; i < parser->alias_count; i++) cxgn_alias_free(&parser->aliases[i]);
    free(parser->structs);
    free(parser->enums);
    free(parser->parsed_files);
    free(parser->aliases);
    cxgn_string_utils_free(parser->utils);
    free(parser);
}

static bool cxgn_parse_header_content(cxgn_struct_parser* parser,
                                 const char* source_name,
                                 char* content,
                                 size_t read,
                                 cxgn_error* err) {
    char* base_dir = NULL;
    size_t pos = 0;

    if (!parser || !source_name || !content) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return false;
    }
    if (cxgn_parser_was_file_parsed(parser, source_name)) return true;
    if (!cxgn_parser_mark_file_parsed(parser, source_name)) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }
    if (!cxgn_strip_c_comments(content, err)) return false;

    base_dir = cxgn_get_directory(source_name);
    while (pos < read) {
        const char* p = cxgn_skip_whitespace(content + pos);
        pos = (size_t)(p - content);
        if (pos >= read) break;

        const char* line_end = strchr(p, '\n');
        if (!line_end) line_end = content + read;
        size_t len = (size_t)(line_end - p);
        char* line = cxgn_strndup(p, len);
        if (!line) {
            free(base_dir);
            free(content);
            cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }

        if (*p == '#' && cxgn_starts_with(p, "#include")) {
            cxgn_parse_include(parser, content, base_dir, &pos, err);
        } else if (cxgn_parse_macro_alias(parser, line) || cxgn_parse_typedef_alias(parser, line) || cxgn_parse_scalar_typedef(parser, line)) {
            pos = (size_t)((*line_end == '\n') ? (line_end + 1 - content) : (line_end - content));
        } else if (cxgn_starts_with(p, "typedef enum") || cxgn_starts_with(p, "enum ")) {
            if (!cxgn_parse_enum(parser, content, source_name, &pos, err)) {
                free(line);
                free(base_dir);
                return false;
            }
        } else if (cxgn_starts_with(p, "typedef struct") || cxgn_starts_with(p, "struct ")) {
            if (!cxgn_parse_struct(parser, content, source_name, &pos, err)) {
                free(line);
                free(base_dir);
                return false;
            }
        } else {
            pos = (size_t)((*line_end == '\n') ? (line_end + 1 - content) : (line_end - content));
        }
        free(line);
    }

    free(base_dir);
    return true;
}

bool cxgn_struct_parser_parse_file(cxgn_struct_parser* parser, const char* header_path, cxgn_error* err) {
    cxgn_error_init(err);
    if (!parser || !header_path) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return false;
    }

    FILE* f = fopen(header_path, "r");
    if (!f) {
        cxgn_error_set(err, CXGN_ERR_FILE_NOT_FOUND, "Cannot open header file");
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Failed to read header size");
        return false;
    }

    char* content = (char*)malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    size_t read = fread(content, 1, (size_t)size, f);
    fclose(f);
    content[read] = '\0';

    const bool ok = cxgn_parse_header_content(parser, header_path, content, read, err);
    free(content);
    return ok;
}

bool cxgn_struct_parser_parse_text(cxgn_struct_parser* parser,
                                   const char* header_text,
                                   const char* source_name,
                                   cxgn_error* err) {
    size_t len;
    char* owned = NULL;

    cxgn_error_init(err);
    if (!parser || !header_text || !source_name) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return false;
    }

    len = strlen(header_text);
    owned = cxgn_strdup(header_text);
    if (!owned) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    const bool ok = cxgn_parse_header_content(parser, source_name, owned, len, err);
    free(owned);
    return ok;
}

size_t cxgn_struct_parser_get_struct_count(const cxgn_struct_parser* parser) {
    return parser ? parser->struct_count : 0;
}

const cxgn_struct_info* cxgn_struct_parser_get_struct(const cxgn_struct_parser* parser, size_t index) {
    if (!parser || index >= parser->struct_count) return NULL;
    return &parser->structs[index];
}

const cxgn_struct_info* cxgn_struct_parser_find_struct(const cxgn_struct_parser* parser, const char* name) {
    if (!parser || !name) return NULL;
    for (size_t i = 0; i < parser->struct_count; i++) {
        if (strcmp(parser->structs[i].name, name) == 0) return &parser->structs[i];
    }
    return NULL;
}

bool cxgn_struct_parser_is_builtin_type(const cxgn_struct_parser* parser, const char* type) {
    if (!type) return false;
    if (parser) {
        char* unqualified = NULL;
        const char* match_type = type;
        for (size_t i = 0; i < parser->enum_count; i++) {
            if (strcmp(type, parser->enums[i].name) == 0) return true;
        }
        unqualified = cxgn_strdup(type);
        if (unqualified) {
            cxgn_trim_trailing_whitespace(unqualified);
            while (cxgn_starts_with(unqualified, "const ")) memmove(unqualified, unqualified + 6, strlen(unqualified + 6) + 1);
            match_type = unqualified;
            for (size_t i = 0; i < parser->enum_count; i++) {
                if (strcmp(match_type, parser->enums[i].name) == 0) {
                    free(unqualified);
                    return true;
                }
            }
            free(unqualified);
        }
    }
    for (size_t i = 0; builtin_types[i]; i++) {
        if (strcmp(type, builtin_types[i]) == 0) return true;
    }
    return false;
}

bool cxgn_struct_parser_is_constexpr_friendly(const cxgn_struct_parser* parser, const char* type) {
    return cxgn_struct_parser_is_builtin_type(parser, type);
}

const char* cxgn_struct_get_name(const cxgn_struct_info* info) {
    return info ? info->name : NULL;
}

const char* cxgn_struct_get_defined_in(const cxgn_struct_info* info) {
    return info ? info->defined_in : NULL;
}

size_t cxgn_struct_get_field_count(const cxgn_struct_info* info) {
    return info ? info->field_count : 0;
}

const cxgn_field_info* cxgn_struct_get_field(const cxgn_struct_info* info, size_t index) {
    if (!info || index >= info->field_count) return NULL;
    return &info->fields[index];
}

const cxgn_field_info* cxgn_struct_find_field(const cxgn_struct_info* info, const char* name) {
    if (!info || !name) return NULL;
    for (size_t i = 0; i < info->field_count; i++) {
        if (strcmp(info->fields[i].name, name) == 0) return &info->fields[i];
    }
    return NULL;
}

const char* cxgn_field_get_type(const cxgn_field_info* field) {
    return field ? field->type : NULL;
}

const char* cxgn_field_get_name(const cxgn_field_info* field) {
    return field ? field->name : NULL;
}

const char* cxgn_field_get_default(const cxgn_field_info* field) {
    return field ? field->default_value : NULL;
}

bool cxgn_field_is_array(const cxgn_field_info* field) {
    return field ? field->is_array : false;
}

const char* cxgn_field_get_array_element_type(const cxgn_field_info* field) {
    return field ? field->array_elem_type : NULL;
}

bool cxgn_field_is_optional(const cxgn_field_info* field) {
    return field ? field->is_optional : false;
}

const char* cxgn_field_get_optional_value_type(const cxgn_field_info* field) {
    return field ? field->optional_value_type : NULL;
}
