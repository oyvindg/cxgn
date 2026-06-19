/**
 * @file yaml_document.c
 * @brief YAML to normalized cxgn_document adapter.
 */

#include "internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <yaml.h>

static const char* yaml_tag_null = "tag:yaml.org,2002:null";
static const char* yaml_tag_bool = "tag:yaml.org,2002:bool";
static const char* yaml_tag_int = "tag:yaml.org,2002:int";
static const char* yaml_tag_float = "tag:yaml.org,2002:float";

typedef enum {
    YAML_SCALAR_NULL = 0,
    YAML_SCALAR_BOOL,
    YAML_SCALAR_INT,
    YAML_SCALAR_FLOAT,
    YAML_SCALAR_STRING
} yaml_scalar_kind;

static void cxgn_set_node_location(cxgn_node* node, const yaml_node_t* yaml_node) {
    if (!node || !yaml_node) return;
    cxgn_node_set_location(node,
                           (size_t)yaml_node->start_mark.line + 1,
                           (size_t)yaml_node->start_mark.column + 1);
}

static bool cxgn_is_false_like(const char* value) {
    return strcmp(value, "false") == 0 ||
           strcmp(value, "False") == 0 ||
           strcmp(value, "FALSE") == 0 ||
           strcmp(value, "no") == 0 ||
           strcmp(value, "No") == 0 ||
           strcmp(value, "NO") == 0 ||
           strcmp(value, "off") == 0 ||
           strcmp(value, "Off") == 0 ||
           strcmp(value, "OFF") == 0;
}

static yaml_scalar_kind cxgn_detect_plain_scalar_kind(const char* value) {
    if (!value || !*value) return YAML_SCALAR_NULL;
    if (strcmp(value, "null") == 0 || strcmp(value, "~") == 0) return YAML_SCALAR_NULL;
    if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) return YAML_SCALAR_BOOL;

    bool has_digit = false;
    bool has_dot = false;
    bool has_exponent = false;
    const char* p = value;
    if (*p == '-' || *p == '+') p++;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            has_digit = true;
        } else if (*p == '.') {
            if (has_dot || has_exponent) return YAML_SCALAR_STRING;
            has_dot = true;
        } else if (*p == 'e' || *p == 'E') {
            if (has_exponent || !has_digit) return YAML_SCALAR_STRING;
            has_exponent = true;
        } else {
            return YAML_SCALAR_STRING;
        }
        p++;
    }

    if (!has_digit) return YAML_SCALAR_STRING;
    return has_dot ? YAML_SCALAR_FLOAT : YAML_SCALAR_INT;
}

static void cxgn_set_oom(cxgn_error* err) {
    cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
}

static char* cxgn_yaml_vasprintf(const char* fmt, va_list args) {
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

static void cxgn_set_owned_error(cxgn_error* err,
                            cxgn_error_code code,
                            char* message,
                            char* path,
                            size_t line,
                            size_t column) {
    if (!err) {
        free(message);
        free(path);
        return;
    }

    cxgn_error_clear(err);
    err->code = code;
    err->message = message;
    err->path = path;
    err->line = line;
    err->column = column;
    err->needs_free = true;
}

static void cxgn_set_source_error(cxgn_error* err,
                             cxgn_error_code code,
                             const char* source_name,
                             size_t line,
                             size_t column,
                             const char* fmt, ...) {
    va_list args;
    char* message;
    char* path = NULL;

    va_start(args, fmt);
    message = cxgn_yaml_vasprintf(fmt, args);
    va_end(args);
    if (!message) {
        cxgn_set_oom(err);
        return;
    }

    if (source_name) {
        path = cxgn_strdup(source_name);
        if (!path) {
            free(message);
            cxgn_set_oom(err);
            return;
        }
    }

    cxgn_set_owned_error(err, code, message, path, line, column);
}

static void cxgn_set_yaml_parser_error(cxgn_error* err,
                                  const char* source_name,
                                  const yaml_parser_t* parser) {
    size_t line = 0;
    size_t column = 0;
    const char* problem = "Failed to parse YAML";
    const char* context = NULL;

    if (parser) {
        line = (size_t)parser->problem_mark.line + 1;
        column = (size_t)parser->problem_mark.column + 1;
        if (parser->problem) problem = parser->problem;
        context = parser->context;
    }

    if (context && context[0]) {
        cxgn_set_source_error(err, CXGN_ERR_YAML_ERROR, source_name, line, column,
                         "%s: %s", context, problem);
        return;
    }

    cxgn_set_source_error(err, CXGN_ERR_YAML_ERROR, source_name, line, column, "%s", problem);
}

static cxgn_node* cxgn_convert_yaml_node(yaml_document_t* doc,
                                    yaml_node_t* yaml_node,
                                    const char* source_name,
                                    cxgn_error* err);

static cxgn_node* cxgn_convert_scalar_node(yaml_node_t* yaml_node,
                                      const char* source_name,
                                      cxgn_error* err) {
    const char* value = (const char*)yaml_node->data.scalar.value;
    const char* tag = (const char*)yaml_node->tag;
    cxgn_node* node = NULL;
    yaml_scalar_kind kind = YAML_SCALAR_STRING;

    if (yaml_node->data.scalar.style == YAML_PLAIN_SCALAR_STYLE) {
        if (tag && strcmp(tag, yaml_tag_null) == 0) kind = YAML_SCALAR_NULL;
        else if (tag && strcmp(tag, yaml_tag_bool) == 0) kind = YAML_SCALAR_BOOL;
        else if (tag && strcmp(tag, yaml_tag_int) == 0) kind = YAML_SCALAR_INT;
        else if (tag && strcmp(tag, yaml_tag_float) == 0) kind = YAML_SCALAR_FLOAT;
        else kind = cxgn_detect_plain_scalar_kind(value);
    }

    if (kind == YAML_SCALAR_NULL) {
        node = cxgn_node_new_null();
    } else if (kind == YAML_SCALAR_BOOL) {
        node = cxgn_node_new_bool(!cxgn_is_false_like(value));
    } else if (kind == YAML_SCALAR_INT) {
        char* end = NULL;
        errno = 0;
        long long parsed = strtoll(value, &end, 0);
        if (errno == 0 && end && *end == '\0') {
            node = cxgn_node_new_integer(parsed);
        } else {
            node = cxgn_node_new_string(value, strlen(value));
        }
    } else if (kind == YAML_SCALAR_FLOAT) {
        char* end = NULL;
        errno = 0;
        double parsed = strtod(value, &end);
        if (errno == 0 && end && *end == '\0') {
            node = cxgn_node_new_float(parsed);
        } else {
            node = cxgn_node_new_string(value, strlen(value));
        }
    } else {
        node = cxgn_node_new_string(value, strlen(value));
    }

    if (!node) {
        (void)source_name;
        cxgn_set_oom(err);
        return NULL;
    }

    cxgn_set_node_location(node, yaml_node);
    if (!cxgn_node_set_raw_scalar_text(node, value, strlen(value))) {
        cxgn_node_free(node);
        cxgn_set_oom(err);
        return NULL;
    }
    return node;
}

static cxgn_node* cxgn_convert_sequence_node(yaml_document_t* doc,
                                        yaml_node_t* yaml_node,
                                        const char* source_name,
                                        cxgn_error* err) {
    cxgn_node* node = cxgn_node_new_array();
    if (!node) {
        cxgn_set_oom(err);
        return NULL;
    }

    cxgn_set_node_location(node, yaml_node);
    for (yaml_node_item_t* item = yaml_node->data.sequence.items.start;
         item < yaml_node->data.sequence.items.top;
         item++) {
        yaml_node_t* child_yaml = yaml_document_get_node(doc, *item);
        cxgn_node* child = child_yaml ? cxgn_convert_yaml_node(doc, child_yaml, source_name, err) : NULL;
        if (!child) {
            cxgn_node_free(node);
            return NULL;
        }
        if (!cxgn_node_array_append(node, child)) {
            cxgn_node_free(child);
            cxgn_node_free(node);
            cxgn_set_oom(err);
            return NULL;
        }
    }

    return node;
}

static cxgn_node* cxgn_convert_mapping_node(yaml_document_t* doc,
                                       yaml_node_t* yaml_node,
                                       const char* source_name,
                                       cxgn_error* err) {
    cxgn_node* node = cxgn_node_new_object();
    if (!node) {
        cxgn_set_oom(err);
        return NULL;
    }

    cxgn_set_node_location(node, yaml_node);
    for (yaml_node_pair_t* pair = yaml_node->data.mapping.pairs.start;
         pair < yaml_node->data.mapping.pairs.top;
         pair++) {
        yaml_node_t* key_yaml = yaml_document_get_node(doc, pair->key);
        yaml_node_t* value_yaml = yaml_document_get_node(doc, pair->value);
        if (!key_yaml || key_yaml->type != YAML_SCALAR_NODE || !value_yaml) {
            size_t line = (size_t)yaml_node->start_mark.line + 1;
            size_t column = (size_t)yaml_node->start_mark.column + 1;
            if (key_yaml) {
                line = (size_t)key_yaml->start_mark.line + 1;
                column = (size_t)key_yaml->start_mark.column + 1;
            }
            cxgn_node_free(node);
            cxgn_set_source_error(err, CXGN_ERR_PARSE_ERROR, source_name, line, column,
                             "Only scalar mapping keys are supported");
            return NULL;
        }

        cxgn_node* value = cxgn_convert_yaml_node(doc, value_yaml, source_name, err);
        if (!value) {
            cxgn_node_free(node);
            return NULL;
        }

        if (!cxgn_node_object_append(node,
                                     (const char*)key_yaml->data.scalar.value,
                                     value,
                                     (size_t)key_yaml->start_mark.line + 1,
                                     (size_t)key_yaml->start_mark.column + 1)) {
            cxgn_node_free(value);
            cxgn_node_free(node);
            cxgn_set_oom(err);
            return NULL;
        }
    }

    return node;
}

static cxgn_node* cxgn_convert_yaml_node(yaml_document_t* doc,
                                    yaml_node_t* yaml_node,
                                    const char* source_name,
                                    cxgn_error* err) {
    if (!yaml_node) {
        cxgn_set_source_error(err, CXGN_ERR_PARSE_ERROR, source_name, 0, 0, "Missing YAML node");
        return NULL;
    }

    switch (yaml_node->type) {
        case YAML_SCALAR_NODE:
            return cxgn_convert_scalar_node(yaml_node, source_name, err);
        case YAML_SEQUENCE_NODE:
            return cxgn_convert_sequence_node(doc, yaml_node, source_name, err);
        case YAML_MAPPING_NODE:
            return cxgn_convert_mapping_node(doc, yaml_node, source_name, err);
        default:
            cxgn_set_source_error(err, CXGN_ERR_PARSE_ERROR, source_name,
                             (size_t)yaml_node->start_mark.line + 1,
                             (size_t)yaml_node->start_mark.column + 1,
                             "Unsupported YAML node type");
            return NULL;
    }
}

static cxgn_document* cxgn_document_from_loaded_yaml(yaml_document_t* yaml_doc,
                                                const char* source_name,
                                                cxgn_error* err) {
    cxgn_document* doc = cxgn_document_new(source_name);
    if (!doc) {
        cxgn_set_oom(err);
        return NULL;
    }

    yaml_node_t* root_yaml = yaml_document_get_root_node(yaml_doc);
    if (!root_yaml) {
        return doc;
    }

    cxgn_node* root = cxgn_convert_yaml_node(yaml_doc, root_yaml, source_name, err);
    if (!root) {
        cxgn_document_free(doc);
        return NULL;
    }

    if (!cxgn_document_set_root(doc, root)) {
        cxgn_node_free(root);
        cxgn_document_free(doc);
        cxgn_set_oom(err);
        return NULL;
    }

    return doc;
}

cxgn_document* cxgn_document_from_yaml_file(const char* yaml_path, cxgn_error* err) {
    cxgn_error_init(err);
    if (!yaml_path) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return NULL;
    }

    FILE* f = fopen(yaml_path, "r");
    if (!f) {
        cxgn_error_set(err, CXGN_ERR_FILE_NOT_FOUND, "Cannot open YAML file");
        return NULL;
    }

    yaml_parser_t parser;
    yaml_document_t yaml_doc;
    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        cxgn_set_source_error(err, CXGN_ERR_YAML_ERROR, yaml_path, 0, 0,
                         "Failed to initialize YAML parser");
        return NULL;
    }

    yaml_parser_set_input_file(&parser, f);
    if (!yaml_parser_load(&parser, &yaml_doc)) {
        cxgn_set_yaml_parser_error(err, yaml_path, &parser);
        yaml_parser_delete(&parser);
        fclose(f);
        return NULL;
    }

    cxgn_document* doc = cxgn_document_from_loaded_yaml(&yaml_doc, yaml_path, err);
    yaml_document_delete(&yaml_doc);
    yaml_parser_delete(&parser);
    fclose(f);
    return doc;
}

cxgn_document* cxgn_document_from_yaml_text(const char* yaml_text,
                                            const char* source_name,
                                            cxgn_error* err) {
    cxgn_error_init(err);
    if (!yaml_text) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return NULL;
    }

    yaml_parser_t parser;
    yaml_document_t yaml_doc;
    if (!yaml_parser_initialize(&parser)) {
        const char* effective_name = (source_name && source_name[0]) ? source_name : "<in-memory-yaml>";
        cxgn_set_source_error(err, CXGN_ERR_YAML_ERROR, effective_name, 0, 0,
                         "Failed to initialize YAML parser");
        return NULL;
    }

    yaml_parser_set_input_string(&parser, (const unsigned char*)yaml_text, strlen(yaml_text));
    if (!yaml_parser_load(&parser, &yaml_doc)) {
        const char* effective_name = (source_name && source_name[0]) ? source_name : "<in-memory-yaml>";
        cxgn_set_yaml_parser_error(err, effective_name, &parser);
        yaml_parser_delete(&parser);
        return NULL;
    }

    const char* effective_name = (source_name && source_name[0]) ? source_name : "<in-memory-yaml>";
    cxgn_document* doc = cxgn_document_from_loaded_yaml(&yaml_doc, effective_name, err);
    yaml_document_delete(&yaml_doc);
    yaml_parser_delete(&parser);
    return doc;
}
