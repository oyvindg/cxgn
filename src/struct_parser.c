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
    "uint32_t", "uint64_t", "const char*", NULL
};

static const char* skip_whitespace(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static bool starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static size_t extract_identifier(const char* s) {
    size_t len = 0;
    if (isalpha((unsigned char)s[0]) || s[0] == '_') {
        len = 1;
        while (isalnum((unsigned char)s[len]) || s[len] == '_') len++;
    }
    return len;
}

static void trim_trailing_whitespace(char* s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static void field_info_free(cxgn_field_info* field) {
    if (!field) return;
    free(field->name);
    free(field->type);
    free(field->default_value);
    free(field->array_elem_type);
    free(field->optional_value_type);
    for (size_t i = 0; i < field->variant_type_count; i++) free(field->variant_types[i]);
    free(field->variant_types);
}

static void struct_info_free(cxgn_struct_info* info) {
    if (!info) return;
    free(info->name);
    free(info->defined_in);
    for (size_t i = 0; i < info->field_count; i++) field_info_free(&info->fields[i]);
    free(info->fields);
}

static void alias_free(cxgn_type_alias* alias) {
    if (!alias) return;
    free(alias->name);
    free(alias->value_type);
}

static cxgn_struct_info* parser_add_struct(cxgn_struct_parser* parser, const char* name, const char* file) {
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
        struct_info_free(info);
        memset(info, 0, sizeof(*info));
        parser->struct_count--;
        return NULL;
    }
    return info;
}

static cxgn_field_info* struct_add_field(cxgn_struct_info* info) {
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

static bool parser_was_file_parsed(cxgn_struct_parser* parser, const char* path) {
    for (size_t i = 0; i < parser->parsed_file_count; i++) {
        if (strcmp(parser->parsed_files[i], path) == 0) return true;
    }
    return false;
}

static bool parser_mark_file_parsed(cxgn_struct_parser* parser, const char* path) {
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

static bool parser_add_alias(cxgn_struct_parser* parser, const char* name,
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
        alias_free(alias);
        memset(alias, 0, sizeof(*alias));
        parser->alias_count--;
        return false;
    }
    return true;
}

static bool parse_macro_alias(cxgn_struct_parser* parser, const char* line) {
    const char* kind = NULL;
    cxgn_type_alias_kind alias_kind = 0;
    if (starts_with(line, "CXGN_ARRAY_TYPEDEF(")) {
        kind = "CXGN_ARRAY_TYPEDEF(";
        alias_kind = CXGN_ALIAS_ARRAY;
    } else if (starts_with(line, "CXGN_OPTIONAL_TYPEDEF(")) {
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

    trim_trailing_whitespace(value_type);
    trim_trailing_whitespace(alias_name);
    char* alias_trim = alias_name;
    while (*alias_trim && isspace((unsigned char)*alias_trim)) alias_trim++;
    const bool ok = parser_add_alias(parser, alias_trim, skip_whitespace(value_type), alias_kind);
    free(value_type);
    free(alias_name);
    return ok;
}

static bool parse_typedef_alias(cxgn_struct_parser* parser, const char* line) {
    const char* p = skip_whitespace(line);
    if (!starts_with(p, "typedef struct {")) return false;

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
    trim_trailing_whitespace(body);
    trim_trailing_whitespace(alias);
    char* alias_trim = alias;
    while (*alias_trim && isspace((unsigned char)*alias_trim)) alias_trim++;

    bool ok = false;
    if (strstr(body, " size_t count;")) {
        const char* prefix = strstr(body, "const ");
        const char* star = prefix ? strstr(prefix, "* data;") : NULL;
        if (prefix && star && prefix < star) {
            char* value_type = cxgn_strndup(prefix, (size_t)(star - prefix));
            if (value_type) {
                trim_trailing_whitespace(value_type);
                ok = parser_add_alias(parser, alias_trim, value_type, CXGN_ALIAS_ARRAY);
                free(value_type);
            }
        }
    } else if (strstr(body, " bool has_value;")) {
        const char* value_end = strstr(body, " value;");
        if (value_end) {
            char* value_type = cxgn_strndup(body, (size_t)(value_end - body));
            if (value_type) {
                trim_trailing_whitespace(value_type);
                ok = parser_add_alias(parser, skip_whitespace(alias_trim), skip_whitespace(value_type),
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
 * Does NOT handle "typedef struct" (handled by parse_typedef_alias).
 */
static bool parse_scalar_typedef(cxgn_struct_parser* parser, const char* line) {
    const char* p = skip_whitespace(line);
    if (!starts_with(p, "typedef ")) return false;
    p += strlen("typedef ");
    p = skip_whitespace(p);
    if (starts_with(p, "struct ")) return false; /* handled elsewhere */
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
    const bool ok = parser_add_alias(parser, alias_name, value_type, CXGN_ALIAS_SCALAR);
    free(value_type);
    free(alias_name);
    return ok;
}

static bool parse_field_line(const cxgn_struct_parser* parser, const char* line, cxgn_field_info* field) {
    const char* p = skip_whitespace(line);
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
        field_info_free(field);
        memset(field, 0, sizeof(*field));
        return false;
    }
    trim_trailing_whitespace(field->type);

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

static bool has_multi_decl_comma(const char* line) {
    int paren = 0;
    for (const char* p = line; *p && *p != ';'; p++) {
        if (*p == '(') paren++;
        else if (*p == ')') paren--;
        else if (*p == ',' && paren == 0) return true;
    }
    return false;
}

static bool parse_multi_field_line(const cxgn_struct_parser* parser, const char* line,
                                   cxgn_struct_info* info) {
    const char* semi = strchr(line, ';');
    const char* comma = strchr(line, ',');
    if (!semi || !comma || comma > semi) return false;

    char synth[CXGN_LINE_SIZE];
    size_t prefix_len = (size_t)(comma - line);
    if (prefix_len + 2 >= sizeof(synth)) return false;
    memcpy(synth, line, prefix_len);
    synth[prefix_len] = ';';
    synth[prefix_len + 1] = '\0';

    cxgn_field_info first = {0};
    if (!parse_field_line(parser, synth, &first)) return false;
    cxgn_field_info* target = struct_add_field(info);
    if (!target) {
        field_info_free(&first);
        return false;
    }
    *target = first;

    const char* type_str = first.type;
    const char* cur = comma + 1;
    while (cur < semi) {
        cur = skip_whitespace(cur);
        const char* next = cur;
        while (next < semi && *next != ',') next++;
        const char* trimmed_end = next;
        while (trimmed_end > cur && isspace((unsigned char)*(trimmed_end - 1))) trimmed_end--;
        if (trimmed_end > cur) {
            size_t type_len = strlen(type_str);
            size_t name_len = (size_t)(trimmed_end - cur);
            if (type_len + 1 + name_len + 2 < sizeof(synth)) {
                memcpy(synth, type_str, type_len);
                synth[type_len] = ' ';
                memcpy(synth + type_len + 1, cur, name_len);
                synth[type_len + 1 + name_len] = ';';
                synth[type_len + 1 + name_len + 1] = '\0';
                cxgn_field_info extra = {0};
                if (parse_field_line(parser, synth, &extra)) {
                    cxgn_field_info* slot = struct_add_field(info);
                    if (slot) *slot = extra;
                    else field_info_free(&extra);
                }
            }
        }
        cur = next + 1;
    }

    return true;
}

static bool parse_struct(cxgn_struct_parser* parser, const char* content, const char* file,
                         size_t* pos, cxgn_error* err) {
    const char* p = content + *pos;
    bool has_typedef = false;
    if (starts_with(p, "typedef")) {
        has_typedef = true;
        p = skip_whitespace(p + strlen("typedef"));
    }
    if (!starts_with(p, "struct")) return false;
    p = skip_whitespace(p + strlen("struct"));

    char* name = NULL;
    size_t name_len = extract_identifier(p);
    if (name_len > 0) {
        name = cxgn_strndup(p, name_len);
        p += name_len;
        p = skip_whitespace(p);
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
    const char* after = skip_whitespace(p);
    if (!name && has_typedef) {
        size_t alias_len = extract_identifier(after);
        if (alias_len == 0) {
            cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Expected typedef struct name");
            return false;
        }
        name = cxgn_strndup(after, alias_len);
        after = skip_whitespace(after + alias_len);
    }

    if (!name) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Expected struct name");
        return false;
    }

    if (*after != ';') {
        while (*after && *after != ';') after++;
    }
    if (*after == ';') after++;

    cxgn_struct_info* info = parser_add_struct(parser, name, file);
    free(name);
    if (!info) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    const char* line = body_start;
    while (line < close_brace) {
        const char* line_end = memchr(line, '\n', (size_t)(close_brace - line));
        if (!line_end) line_end = close_brace;

        char buf[CXGN_LINE_SIZE];
        size_t len = (size_t)(line_end - line);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, line, len);
        buf[len] = '\0';

        if (has_multi_decl_comma(buf)) parse_multi_field_line(parser, buf, info);
        else {
            cxgn_field_info field = {0};
            if (parse_field_line(parser, buf, &field)) {
                cxgn_field_info* slot = struct_add_field(info);
                if (slot) *slot = field;
                else field_info_free(&field);
            }
        }

        line = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    *pos = (size_t)(after - content);
    return true;
}

static bool parse_include(cxgn_struct_parser* parser, const char* content, const char* base_dir,
                          size_t* pos, cxgn_error* err) {
    const char* p = skip_whitespace(content + *pos + strlen("#include"));
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
    if (full && !parser_was_file_parsed(parser, full)) cxgn_struct_parser_parse_file(parser, full, err);
    free(full);

    while (*end && *end != '\n') end++;
    *pos = (size_t)(end - content);
    return true;
}

cxgn_struct_parser* cxgn_struct_parser_new(const cxgn_string_utils* utils) {
    cxgn_struct_parser* parser = (cxgn_struct_parser*)calloc(1, sizeof(*parser));
    if (!parser) return NULL;
    parser->utils = utils;
    parser->struct_capacity = 8;
    parser->parsed_file_capacity = 8;
    parser->alias_capacity = 8;
    parser->structs = (cxgn_struct_info*)calloc(parser->struct_capacity, sizeof(*parser->structs));
    parser->parsed_files = (char**)calloc(parser->parsed_file_capacity, sizeof(*parser->parsed_files));
    parser->aliases = (cxgn_type_alias*)calloc(parser->alias_capacity, sizeof(*parser->aliases));
    if (!parser->structs || !parser->parsed_files || !parser->aliases) {
        cxgn_struct_parser_free(parser);
        return NULL;
    }
    return parser;
}

void cxgn_struct_parser_free(cxgn_struct_parser* parser) {
    if (!parser) return;
    for (size_t i = 0; i < parser->struct_count; i++) struct_info_free(&parser->structs[i]);
    for (size_t i = 0; i < parser->parsed_file_count; i++) free(parser->parsed_files[i]);
    for (size_t i = 0; i < parser->alias_count; i++) alias_free(&parser->aliases[i]);
    free(parser->structs);
    free(parser->parsed_files);
    free(parser->aliases);
    free(parser);
}

bool cxgn_struct_parser_parse_file(cxgn_struct_parser* parser, const char* header_path, cxgn_error* err) {
    cxgn_error_init(err);
    if (!parser || !header_path) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return false;
    }
    if (parser_was_file_parsed(parser, header_path)) return true;

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

    if (!parser_mark_file_parsed(parser, header_path)) {
        free(content);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    char* base_dir = cxgn_get_directory(header_path);
    size_t pos = 0;
    while (pos < read) {
        const char* p = skip_whitespace(content + pos);
        pos = (size_t)(p - content);
        if (pos >= read) break;

        const char* line_end = strchr(p, '\n');
        if (!line_end) line_end = content + read;
        char line[CXGN_LINE_SIZE];
        size_t len = (size_t)(line_end - p);
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = '\0';

        if (*p == '#' && starts_with(p, "#include")) {
            parse_include(parser, content, base_dir, &pos, err);
        } else if (parse_macro_alias(parser, line) || parse_typedef_alias(parser, line) || parse_scalar_typedef(parser, line)) {
            pos = (size_t)((*line_end == '\n') ? (line_end + 1 - content) : (line_end - content));
        } else if (starts_with(p, "typedef struct") || starts_with(p, "struct ")) {
            if (!parse_struct(parser, content, header_path, &pos, err)) {
                free(base_dir);
                free(content);
                return false;
            }
        } else {
            pos = (size_t)((*line_end == '\n') ? (line_end + 1 - content) : (line_end - content));
        }
    }

    free(base_dir);
    free(content);
    return true;
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
    (void)parser;
    if (!type) return false;
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

bool cxgn_field_is_variant(const cxgn_field_info* field) {
    return field ? field->is_variant : false;
}

size_t cxgn_field_get_variant_type_count(const cxgn_field_info* field) {
    return field ? field->variant_type_count : 0;
}

const char* cxgn_field_get_variant_type(const cxgn_field_info* field, size_t index) {
    if (!field || index >= field->variant_type_count) return NULL;
    return field->variant_types[index];
}
