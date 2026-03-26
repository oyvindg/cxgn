/**
 * @file struct_parser.c
 * @brief C++ header parser for struct definitions.
 *
 * Parses C++ header files to extract struct definitions, field types,
 * default values, and follows #include directives.
 */

#include "internal.h"
#include <stdio.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin Types
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char* const builtin_types[] = {
    "int",
    "unsigned int",
    "long",
    "unsigned long",
    "long long",
    "unsigned long long",
    "short",
    "unsigned short",
    "char",
    "unsigned char",
    "signed char",
    "float",
    "double",
    "long double",
    "bool",
    "size_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "std::string",
    "std::string_view",
    NULL
};

static const char* const constexpr_friendly_types[] = {
    "int",
    "unsigned int",
    "long",
    "unsigned long",
    "long long",
    "unsigned long long",
    "short",
    "unsigned short",
    "char",
    "unsigned char",
    "signed char",
    "float",
    "double",
    "long double",
    "bool",
    "size_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "std::string_view",
    NULL
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Skip whitespace in a string.
 */
static const char* skip_whitespace(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/**
 * @brief Check if string starts with prefix.
 */
static bool starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/**
 * @brief Extract identifier from string.
 * @return Length of identifier, 0 if no identifier found
 */
static size_t extract_identifier(const char* s) {
    size_t len = 0;
    if (isalpha((unsigned char)s[0]) || s[0] == '_') {
        len = 1;
        while (isalnum((unsigned char)s[len]) || s[len] == '_') {
            len++;
        }
    }
    return len;
}

/**
 * @brief Free a field_info structure.
 */
static void field_info_free(cg_field_info* field) {
    if (!field) return;
    free(field->name);
    free(field->type);
    free(field->default_value);
    free(field->array_elem_type);
    free(field->optional_value_type);
    free(field->oneof_type_a);
    free(field->oneof_type_b);
}

/**
 * @brief Free a struct_info structure.
 */
static void struct_info_free(cg_struct_info* info) {
    if (!info) return;
    free(info->name);
    free(info->defined_in);
    if (info->fields) {
        for (size_t i = 0; i < info->field_count; i++) {
            field_info_free(&info->fields[i]);
        }
        free(info->fields);
    }
}

/**
 * @brief Add a struct to the parser.
 */
static cg_struct_info* parser_add_struct(cg_struct_parser* parser, const char* name, const char* file) {
    if (parser->struct_count >= parser->struct_capacity) {
        size_t new_cap = parser->struct_capacity * 2;
        if (new_cap < 8) new_cap = 8;
        cg_struct_info* new_structs = (cg_struct_info*)realloc(
            parser->structs, new_cap * sizeof(cg_struct_info));
        if (!new_structs) return NULL;
        parser->structs = new_structs;
        parser->struct_capacity = new_cap;
    }

    cg_struct_info* info = &parser->structs[parser->struct_count];
    memset(info, 0, sizeof(*info));
    info->name = cg_strdup(name);
    info->defined_in = cg_strdup(file);
    info->field_capacity = 8;
    info->fields = (cg_field_info*)calloc(info->field_capacity, sizeof(cg_field_info));
    if (!info->name || !info->defined_in || !info->fields) {
        struct_info_free(info);
        return NULL;
    }

    parser->struct_count++;
    return info;
}

/**
 * @brief Add a field to a struct.
 */
static cg_field_info* struct_add_field(cg_struct_info* info) {
    if (info->field_count >= info->field_capacity) {
        size_t new_cap = info->field_capacity * 2;
        cg_field_info* new_fields = (cg_field_info*)realloc(
            info->fields, new_cap * sizeof(cg_field_info));
        if (!new_fields) return NULL;
        info->fields = new_fields;
        info->field_capacity = new_cap;
    }

    cg_field_info* field = &info->fields[info->field_count];
    memset(field, 0, sizeof(*field));
    info->field_count++;
    return field;
}

/**
 * @brief Parse type template parameter (e.g., extract T from Array<T>).
 */
static char* extract_template_param(const char* type, const char* wrapper) {
    size_t wrapper_len = strlen(wrapper);
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

    /* Trim whitespace */
    while (*start && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*(end - 1))) end--;

    return cg_strndup(start, (size_t)(end - start));
}

/**
 * @brief Extract two template params from OneOf<A, B>.
 */
static bool extract_two_template_params(const char* type, const char* wrapper,
                                        char** out_a, char** out_b) {
    if (!out_a || !out_b) return false;
    *out_a = NULL;
    *out_b = NULL;

    char* payload = extract_template_param(type, wrapper);
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

/**
 * @brief Check if file was already parsed.
 */
static bool parser_was_file_parsed(cg_struct_parser* parser, const char* path) {
    for (size_t i = 0; i < parser->parsed_file_count; i++) {
        if (strcmp(parser->parsed_files[i], path) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Mark file as parsed.
 */
static bool parser_mark_file_parsed(cg_struct_parser* parser, const char* path) {
    if (parser->parsed_file_count >= parser->parsed_file_capacity) {
        size_t new_cap = parser->parsed_file_capacity * 2;
        if (new_cap < 8) new_cap = 8;
        char** new_files = (char**)realloc(
            parser->parsed_files, new_cap * sizeof(char*));
        if (!new_files) return false;
        parser->parsed_files = new_files;
        parser->parsed_file_capacity = new_cap;
    }

    parser->parsed_files[parser->parsed_file_count] = cg_strdup(path);
    if (!parser->parsed_files[parser->parsed_file_count]) return false;
    parser->parsed_file_count++;
    return true;
}

/**
 * @brief Parse a single field line.
 */
static bool parse_field_line(const char* line, cg_field_info* field) {
    const char* p = skip_whitespace(line);
    if (!*p || *p == '/' || *p == '#' || *p == '}') return false;

    /* Find the type - everything up to the last identifier before ; or = */
    const char* line_end = strchr(p, ';');
    if (!line_end) return false;

    const char* default_start = strchr(p, '=');
    const char* name_end = default_start ? default_start : line_end;

    /* Trim trailing whitespace from name_end */
    while (name_end > p && isspace((unsigned char)*(name_end - 1))) name_end--;

    /* Find the field name (last identifier) */
    const char* name_start = name_end;
    while (name_start > p && !isspace((unsigned char)*(name_start - 1))) name_start--;

    if (name_start == name_end) return false;

    size_t name_len = (size_t)(name_end - name_start);
    field->name = cg_strndup(name_start, name_len);
    if (!field->name) return false;

    /* Extract type (everything before the name) */
    const char* type_end = name_start;
    while (type_end > p && isspace((unsigned char)*(type_end - 1))) type_end--;

    if (type_end <= p) {
        free(field->name);
        field->name = NULL;
        return false;
    }

    size_t type_len = (size_t)(type_end - p);
    field->type = cg_strndup(p, type_len);
    if (!field->type) {
        free(field->name);
        field->name = NULL;
        return false;
    }

    /* Check for Array<T> */
    char* array_elem = extract_template_param(field->type, "Array");
    if (array_elem) {
        field->is_array = true;
        field->array_elem_type = array_elem;
    }

    /* Check for Optional<T> */
    char* optional_elem = extract_template_param(field->type, "Optional");
    if (optional_elem) {
        field->is_optional = true;
        field->optional_value_type = optional_elem;
    }

    /* Check for OneOf<A, B> */
    char* oneof_a = NULL;
    char* oneof_b = NULL;
    if (extract_two_template_params(field->type, "OneOf", &oneof_a, &oneof_b)) {
        field->is_oneof = true;
        field->oneof_type_a = oneof_a;
        field->oneof_type_b = oneof_b;
    }

    /* Parse default value if present */
    if (default_start) {
        default_start++;  /* Skip '=' */
        default_start = skip_whitespace(default_start);
        const char* default_end = line_end;
        while (default_end > default_start && isspace((unsigned char)*(default_end - 1))) {
            default_end--;
        }
        if (default_end > default_start) {
            field->default_value = cg_strndup(default_start, (size_t)(default_end - default_start));
        }
    }

    return true;
}

/**
 * @brief Check if a field line is a multi-declaration (e.g. "float x, y, z;").
 *
 * Only commas at template depth 0 (not inside `<>` or `()`) are considered.
 * This prevents method parameter commas from being treated as field separators.
 *
 * @param line  Null-terminated field line.
 * @return true if a depth-0 comma exists before the semicolon.
 */
static bool has_multi_decl_comma(const char* line) {
    int angle = 0;
    int paren = 0;
    for (const char* p = line; *p && *p != ';'; p++) {
        if (*p == '<') angle++;
        else if (*p == '>') angle--;
        else if (*p == '(') paren++;
        else if (*p == ')') paren--;
        else if (*p == ',' && angle == 0 && paren == 0) return true;
    }
    return false;
}

/**
 * @brief Parse a multi-declaration field line into individual fields.
 *
 * Expands e.g. `float x, y, z;` into three separate fields, each sharing
 * the same type. Commas inside template arguments are ignored.
 * Default values on multi-declarations are not supported and are ignored.
 *
 * @param line  Null-terminated field line.
 * @param info  Struct to append fields to.
 * @return true if at least one field was added.
 */
static bool parse_multi_field_line(const char* line, cg_struct_info* info) {
    const char* p = skip_whitespace(line);
    const char* semicolon = strchr(p, ';');
    if (!semicolon) return false;

    /* Find the first depth-0 comma to delimit "type first_name" */
    const char* first_comma = NULL;
    {
        int depth = 0;
        for (const char* q = p; q < semicolon; q++) {
            if (*q == '<') depth++;
            else if (*q == '>') depth--;
            else if (*q == ',' && depth == 0) { first_comma = q; break; }
        }
    }
    if (!first_comma) return false;

    /* Synthesize "type first_name;" to reuse parse_field_line */
    char synth[CG_LINE_SIZE];
    size_t prefix_len = (size_t)(first_comma - p);
    if (prefix_len + 2 >= sizeof(synth)) return false;
    memcpy(synth, p, prefix_len);
    synth[prefix_len]     = ';';
    synth[prefix_len + 1] = '\0';

    cg_field_info proto;
    memset(&proto, 0, sizeof(proto));
    if (!parse_field_line(synth, &proto)) return false;

    /* type_str points into heap memory owned by proto / first field — safe
     * across struct_add_field reallocs since the string itself is not moved. */
    const char* type_str = proto.type;

    bool added = false;
    cg_field_info* f = struct_add_field(info);
    if (!f) { field_info_free(&proto); return false; }
    *f = proto;
    added = true;

    /* Process each remaining name after the first comma */
    const char* cur = first_comma + 1;
    while (cur < semicolon) {
        cur = skip_whitespace(cur);

        /* Find end of this name: next depth-0 comma or the semicolon */
        const char* name_end = cur;
        {
            int depth = 0;
            while (name_end < semicolon) {
                if (*name_end == '<') depth++;
                else if (*name_end == '>') depth--;
                else if (*name_end == ',' && depth == 0) break;
                name_end++;
            }
        }

        /* Trim trailing whitespace from name */
        const char* trimmed = name_end;
        while (trimmed > cur && isspace((unsigned char)*(trimmed - 1))) trimmed--;

        if (trimmed > cur) {
            size_t type_len = strlen(type_str);
            size_t name_len = (size_t)(trimmed - cur);
            if (type_len + 1 + name_len + 2 < sizeof(synth)) {
                memcpy(synth, type_str, type_len);
                synth[type_len] = ' ';
                memcpy(synth + type_len + 1, cur, name_len);
                synth[type_len + 1 + name_len]     = ';';
                synth[type_len + 1 + name_len + 1] = '\0';

                cg_field_info extra;
                memset(&extra, 0, sizeof(extra));
                if (parse_field_line(synth, &extra)) {
                    cg_field_info* ef = struct_add_field(info);
                    if (ef) { *ef = extra; added = true; }
                    else field_info_free(&extra);
                }
            }
        }

        cur = (*name_end == ',') ? name_end + 1 : semicolon;
    }

    return added;
}

/**
 * @brief Parse a struct from file content.
 */
static bool parse_struct(cg_struct_parser* parser, const char* content, const char* file,
                        size_t* pos, cg_error* err) {
    const char* p = content + *pos;

    /* Skip 'struct' keyword */
    p = skip_whitespace(p + 6);

    /* Extract struct name */
    size_t name_len = extract_identifier(p);
    if (name_len == 0) {
        cg_error_set(err, CG_ERR_PARSE_ERROR, "Expected struct name");
        return false;
    }

    char* name = cg_strndup(p, name_len);
    if (!name) {
        cg_error_set(err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    p += name_len;
    p = skip_whitespace(p);

    /* Find opening brace */
    if (*p != '{') {
        /* Forward declaration - skip */
        free(name);
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
        *pos = (size_t)(p - content);
        return true;
    }
    p++;  /* Skip '{' */

    /* Add struct */
    cg_struct_info* info = parser_add_struct(parser, name, file);
    free(name);
    if (!info) {
        cg_error_set(err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    /* Parse fields until closing brace */
    char line_buf[CG_LINE_SIZE];
    while (*p && *p != '}') {
        /* Find end of line */
        const char* line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);

        size_t line_len = (size_t)(line_end - p);
        if (line_len >= sizeof(line_buf)) line_len = sizeof(line_buf) - 1;
        memcpy(line_buf, p, line_len);
        line_buf[line_len] = '\0';

        /* Try to parse field — multi-declaration (float x, y, z;) or single */
        if (has_multi_decl_comma(line_buf)) {
            parse_multi_field_line(line_buf, info);
        } else {
            cg_field_info temp_field;
            memset(&temp_field, 0, sizeof(temp_field));
            if (parse_field_line(line_buf, &temp_field)) {
                cg_field_info* field = struct_add_field(info);
                if (field) {
                    *field = temp_field;
                } else {
                    field_info_free(&temp_field);
                }
            }
        }

        p = *line_end ? line_end + 1 : line_end;
    }

    if (*p == '}') p++;
    if (*p == ';') p++;

    *pos = (size_t)(p - content);
    return true;
}

/**
 * @brief Parse a #include directive.
 */
static bool parse_include(cg_struct_parser* parser, const char* content, const char* base_dir,
                          size_t* pos, cg_error* err) {
    const char* p = content + *pos;

    /* Skip #include */
    p = skip_whitespace(p + 8);

    char delimiter = *p;
    if (delimiter != '"' && delimiter != '<') {
        /* Skip to end of line */
        while (*p && *p != '\n') p++;
        *pos = (size_t)(p - content);
        return true;
    }

    p++;  /* Skip delimiter */

    char end_delim = (delimiter == '"') ? '"' : '>';
    const char* path_end = strchr(p, end_delim);
    if (!path_end) {
        while (*p && *p != '\n') p++;
        *pos = (size_t)(p - content);
        return true;
    }

    /* Only follow local includes (quoted) */
    if (delimiter == '"') {
        char* include_path = cg_strndup(p, (size_t)(path_end - p));
        if (include_path) {
            char* full_path = cg_path_join(base_dir, include_path);
            free(include_path);

            if (full_path && !parser_was_file_parsed(parser, full_path)) {
                cg_struct_parser_parse_file(parser, full_path, err);
            }
            free(full_path);
        }
    }

    p = path_end + 1;
    while (*p && *p != '\n') p++;
    *pos = (size_t)(p - content);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

cg_struct_parser* cg_struct_parser_new(const cg_string_utils* utils) {
    cg_struct_parser* parser = (cg_struct_parser*)calloc(1, sizeof(cg_struct_parser));
    if (!parser) return NULL;

    parser->utils = utils;
    parser->struct_capacity = 8;
    parser->structs = (cg_struct_info*)calloc(parser->struct_capacity, sizeof(cg_struct_info));
    parser->parsed_file_capacity = 8;
    parser->parsed_files = (char**)calloc(parser->parsed_file_capacity, sizeof(char*));

    if (!parser->structs || !parser->parsed_files) {
        cg_struct_parser_free(parser);
        return NULL;
    }

    return parser;
}

void cg_struct_parser_free(cg_struct_parser* parser) {
    if (!parser) return;

    if (parser->structs) {
        for (size_t i = 0; i < parser->struct_count; i++) {
            struct_info_free(&parser->structs[i]);
        }
        free(parser->structs);
    }

    if (parser->parsed_files) {
        for (size_t i = 0; i < parser->parsed_file_count; i++) {
            free(parser->parsed_files[i]);
        }
        free(parser->parsed_files);
    }

    free(parser);
}

bool cg_struct_parser_parse_file(cg_struct_parser* parser, const char* header_path, cg_error* err) {
    cg_error_init(err);

    if (!parser || !header_path) {
        cg_error_set(err, CG_ERR_PARSE_ERROR, "Invalid arguments");
        return false;
    }

    /* Check if already parsed */
    if (parser_was_file_parsed(parser, header_path)) {
        return true;
    }

    /* Open and read file */
    FILE* f = fopen(header_path, "r");
    if (!f) {
        cg_error_set(err, CG_ERR_FILE_NOT_FOUND, "Cannot open header file");
        return false;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        parser_mark_file_parsed(parser, header_path);
        return true;
    }

    /* Read content */
    char* content = (char*)malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        cg_error_set(err, CG_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    size_t read = fread(content, 1, (size_t)size, f);
    fclose(f);
    content[read] = '\0';

    /* Mark as parsed before recursing (to handle circular includes) */
    parser_mark_file_parsed(parser, header_path);

    /* Get base directory for includes */
    char* base_dir = cg_get_directory(header_path);

    /* Parse content */
    size_t pos = 0;
    while (pos < read) {
        const char* p = skip_whitespace(content + pos);
        pos = (size_t)(p - content);

        if (pos >= read) break;

        if (*p == '#' && starts_with(p, "#include")) {
            parse_include(parser, content, base_dir, &pos, err);
        } else if (starts_with(p, "struct ")) {
            parse_struct(parser, content, header_path, &pos, err);
        } else {
            /* Skip to next line */
            while (content[pos] && content[pos] != '\n') pos++;
            if (content[pos] == '\n') pos++;
        }
    }

    free(base_dir);
    free(content);
    return true;
}

size_t cg_struct_parser_get_struct_count(const cg_struct_parser* parser) {
    return parser ? parser->struct_count : 0;
}

const cg_struct_info* cg_struct_parser_get_struct(const cg_struct_parser* parser, size_t index) {
    if (!parser || index >= parser->struct_count) return NULL;
    return &parser->structs[index];
}

const cg_struct_info* cg_struct_parser_find_struct(const cg_struct_parser* parser, const char* name) {
    if (!parser || !name) return NULL;
    for (size_t i = 0; i < parser->struct_count; i++) {
        if (strcmp(parser->structs[i].name, name) == 0) {
            return &parser->structs[i];
        }
    }
    return NULL;
}

bool cg_struct_parser_is_builtin_type(const cg_struct_parser* parser, const char* type) {
    (void)parser;
    if (!type) return false;
    for (size_t i = 0; builtin_types[i]; i++) {
        if (strcmp(type, builtin_types[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool cg_struct_parser_is_constexpr_friendly(const cg_struct_parser* parser, const char* type) {
    (void)parser;
    if (!type) return false;
    for (size_t i = 0; constexpr_friendly_types[i]; i++) {
        if (strcmp(type, constexpr_friendly_types[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Struct Info API
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* cg_struct_get_name(const cg_struct_info* info) {
    return info ? info->name : NULL;
}

const char* cg_struct_get_defined_in(const cg_struct_info* info) {
    return info ? info->defined_in : NULL;
}

size_t cg_struct_get_field_count(const cg_struct_info* info) {
    return info ? info->field_count : 0;
}

const cg_field_info* cg_struct_get_field(const cg_struct_info* info, size_t index) {
    if (!info || index >= info->field_count) return NULL;
    return &info->fields[index];
}

const cg_field_info* cg_struct_find_field(const cg_struct_info* info, const char* name) {
    if (!info || !name) return NULL;
    for (size_t i = 0; i < info->field_count; i++) {
        if (strcmp(info->fields[i].name, name) == 0) {
            return &info->fields[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Field Info API
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* cg_field_get_type(const cg_field_info* field) {
    return field ? field->type : NULL;
}

const char* cg_field_get_name(const cg_field_info* field) {
    return field ? field->name : NULL;
}

const char* cg_field_get_default(const cg_field_info* field) {
    return field ? field->default_value : NULL;
}

bool cg_field_is_array(const cg_field_info* field) {
    return field ? field->is_array : false;
}

const char* cg_field_get_array_element_type(const cg_field_info* field) {
    return field ? field->array_elem_type : NULL;
}

bool cg_field_is_optional(const cg_field_info* field) {
    return field ? field->is_optional : false;
}

const char* cg_field_get_optional_value_type(const cg_field_info* field) {
    return field ? field->optional_value_type : NULL;
}

bool cg_field_is_oneof(const cg_field_info* field) {
    return field ? field->is_oneof : false;
}

const char* cg_field_get_oneof_type_a(const cg_field_info* field) {
    return field ? field->oneof_type_a : NULL;
}

const char* cg_field_get_oneof_type_b(const cg_field_info* field) {
    return field ? field->oneof_type_b : NULL;
}
