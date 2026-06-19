/**
 * @file document.c
 * @brief Internal normalized document/node helpers for cxgn.
 */

#include "internal.h"
#include <stdio.h>

static cxgn_node* cxgn_node_clone_internal(const cxgn_node* node);
static cxgn_node* cxgn_node_merge_internal(const cxgn_node* base_node, const cxgn_node* overlay_node);

static bool cxgn_yaml_buf_append(char** buf, size_t* len, size_t* cap, const char* text) {
    size_t text_len;
    char* next;
    if (!buf || !len || !cap || !text) return false;
    text_len = strlen(text);
    if (*len + text_len + 1 > *cap) {
        size_t new_cap = *cap ? *cap * 2 : 256u;
        while (*len + text_len + 1 > new_cap) new_cap *= 2;
        next = (char*)realloc(*buf, new_cap);
        if (!next) return false;
        *buf = next;
        *cap = new_cap;
    }
    memcpy(*buf + *len, text, text_len + 1);
    *len += text_len;
    return true;
}

static bool cxgn_yaml_buf_append_indent(char** buf, size_t* len, size_t* cap, int indent) {
    for (int i = 0; i < indent; i++) {
        if (!cxgn_yaml_buf_append(buf, len, cap, "  ")) return false;
    }
    return true;
}

static bool cxgn_yaml_buf_append_escaped_string(char** buf, size_t* len, size_t* cap, const char* text) {
    const unsigned char* p = (const unsigned char*)(text ? text : "");
    if (!cxgn_yaml_buf_append(buf, len, cap, "\"")) return false;
    for (; *p; ++p) {
        switch (*p) {
            case '\\':
                if (!cxgn_yaml_buf_append(buf, len, cap, "\\\\")) return false;
                break;
            case '"':
                if (!cxgn_yaml_buf_append(buf, len, cap, "\\\"")) return false;
                break;
            case '\n':
                if (!cxgn_yaml_buf_append(buf, len, cap, "\\n")) return false;
                break;
            case '\r':
                if (!cxgn_yaml_buf_append(buf, len, cap, "\\r")) return false;
                break;
            case '\t':
                if (!cxgn_yaml_buf_append(buf, len, cap, "\\t")) return false;
                break;
            default: {
                char ch[2] = {(char)*p, '\0'};
                if (!cxgn_yaml_buf_append(buf, len, cap, ch)) return false;
                break;
            }
        }
    }
    return cxgn_yaml_buf_append(buf, len, cap, "\"");
}

static bool cxgn_node_to_yaml_append(const cxgn_node* node,
                                     char** buf,
                                     size_t* len,
                                     size_t* cap,
                                     int indent);

static bool cxgn_object_to_yaml_append(const cxgn_node* node,
                                       char** buf,
                                       size_t* len,
                                       size_t* cap,
                                       int indent) {
    for (size_t i = 0; i < node->as.object.count; i++) {
        const cxgn_object_entry* entry = &node->as.object.entries[i];
        if (!cxgn_yaml_buf_append_indent(buf, len, cap, indent)) return false;
        if (!cxgn_yaml_buf_append(buf, len, cap, entry->key ? entry->key : "")) return false;
        if (!cxgn_yaml_buf_append(buf, len, cap, ":")) return false;
        if (entry->value &&
            (entry->value->type == CXGN_NODE_OBJECT || entry->value->type == CXGN_NODE_ARRAY)) {
            if (entry->value->type == CXGN_NODE_ARRAY && entry->value->as.array.count == 0u) {
                if (!cxgn_yaml_buf_append(buf, len, cap, " []\n")) return false;
                continue;
            }
            if (entry->value->type == CXGN_NODE_OBJECT && entry->value->as.object.count == 0u) {
                if (!cxgn_yaml_buf_append(buf, len, cap, " {}\n")) return false;
                continue;
            }
            if (!cxgn_yaml_buf_append(buf, len, cap, "\n")) return false;
            if (!cxgn_node_to_yaml_append(entry->value, buf, len, cap, indent + 1)) return false;
        } else {
            if (!cxgn_yaml_buf_append(buf, len, cap, " ")) return false;
            if (!cxgn_node_to_yaml_append(entry->value, buf, len, cap, indent)) return false;
        }
    }
    return true;
}

static bool cxgn_array_to_yaml_append(const cxgn_node* node,
                                      char** buf,
                                      size_t* len,
                                      size_t* cap,
                                      int indent) {
    if (node->as.array.count == 0) return cxgn_yaml_buf_append(buf, len, cap, "[]\n");
    for (size_t i = 0; i < node->as.array.count; i++) {
        const cxgn_node* item = node->as.array.items[i];
        if (!cxgn_yaml_buf_append_indent(buf, len, cap, indent)) return false;
        if (!cxgn_yaml_buf_append(buf, len, cap, "-")) return false;
        if (item && (item->type == CXGN_NODE_OBJECT || item->type == CXGN_NODE_ARRAY)) {
            if (item->type == CXGN_NODE_OBJECT && item->as.object.count > 0) {
                const cxgn_object_entry* first = &item->as.object.entries[0];
                if (!cxgn_yaml_buf_append(buf, len, cap, " ")) return false;
                if (!cxgn_yaml_buf_append(buf, len, cap, first->key ? first->key : "")) return false;
                if (!cxgn_yaml_buf_append(buf, len, cap, ":")) return false;
                if (first->value &&
                    (first->value->type == CXGN_NODE_OBJECT || first->value->type == CXGN_NODE_ARRAY)) {
                    if (first->value->type == CXGN_NODE_ARRAY && first->value->as.array.count == 0u) {
                        if (!cxgn_yaml_buf_append(buf, len, cap, " []\n")) return false;
                    } else if (first->value->type == CXGN_NODE_OBJECT && first->value->as.object.count == 0u) {
                        if (!cxgn_yaml_buf_append(buf, len, cap, " {}\n")) return false;
                    } else {
                        if (!cxgn_yaml_buf_append(buf, len, cap, "\n")) return false;
                        if (!cxgn_node_to_yaml_append(first->value, buf, len, cap, indent + 2)) return false;
                    }
                } else {
                    if (!cxgn_yaml_buf_append(buf, len, cap, " ")) return false;
                    if (!cxgn_node_to_yaml_append(first->value, buf, len, cap, indent + 1)) return false;
                }
                for (size_t j = 1; j < item->as.object.count; j++) {
                    const cxgn_object_entry* entry = &item->as.object.entries[j];
                    if (!cxgn_yaml_buf_append_indent(buf, len, cap, indent + 1)) return false;
                    if (!cxgn_yaml_buf_append(buf, len, cap, entry->key ? entry->key : "")) return false;
                    if (!cxgn_yaml_buf_append(buf, len, cap, ":")) return false;
                    if (entry->value &&
                        (entry->value->type == CXGN_NODE_OBJECT || entry->value->type == CXGN_NODE_ARRAY)) {
                        if (entry->value->type == CXGN_NODE_ARRAY && entry->value->as.array.count == 0u) {
                            if (!cxgn_yaml_buf_append(buf, len, cap, " []\n")) return false;
                        } else if (entry->value->type == CXGN_NODE_OBJECT && entry->value->as.object.count == 0u) {
                            if (!cxgn_yaml_buf_append(buf, len, cap, " {}\n")) return false;
                        } else {
                            if (!cxgn_yaml_buf_append(buf, len, cap, "\n")) return false;
                            if (!cxgn_node_to_yaml_append(entry->value, buf, len, cap, indent + 2)) return false;
                        }
                    } else {
                        if (!cxgn_yaml_buf_append(buf, len, cap, " ")) return false;
                        if (!cxgn_node_to_yaml_append(entry->value, buf, len, cap, indent + 1)) return false;
                    }
                }
            } else {
                if (!cxgn_yaml_buf_append(buf, len, cap, "\n")) return false;
                if (!cxgn_node_to_yaml_append(item, buf, len, cap, indent + 1)) return false;
            }
        } else {
            if (!cxgn_yaml_buf_append(buf, len, cap, " ")) return false;
            if (!cxgn_node_to_yaml_append(item, buf, len, cap, indent)) return false;
        }
    }
    return true;
}

static bool cxgn_node_to_yaml_append(const cxgn_node* node,
                                     char** buf,
                                     size_t* len,
                                     size_t* cap,
                                     int indent) {
    char number[64];
    bool bv;
    long long iv;
    double fv;
    size_t str_len = 0;
    const char* sv;

    if (!node) return cxgn_yaml_buf_append(buf, len, cap, "null\n");

    switch (node->type) {
        case CXGN_NODE_NULL:
            return cxgn_yaml_buf_append(buf, len, cap, "null\n");
        case CXGN_NODE_BOOL:
            bv = node->as.bool_value;
            return cxgn_yaml_buf_append(buf, len, cap, bv ? "true\n" : "false\n");
        case CXGN_NODE_INTEGER:
            iv = node->as.int_value;
            snprintf(number, sizeof(number), "%lld\n", iv);
            return cxgn_yaml_buf_append(buf, len, cap, number);
        case CXGN_NODE_FLOAT:
            fv = node->as.float_value;
            snprintf(number, sizeof(number), "%.17g\n", fv);
            return cxgn_yaml_buf_append(buf, len, cap, number);
        case CXGN_NODE_STRING:
            sv = cxgn_node_get_string(node, &str_len);
            if (!cxgn_yaml_buf_append_escaped_string(buf, len, cap, sv ? sv : "")) return false;
            return cxgn_yaml_buf_append(buf, len, cap, "\n");
        case CXGN_NODE_ARRAY:
            return cxgn_array_to_yaml_append(node, buf, len, cap, indent);
        case CXGN_NODE_OBJECT:
            return cxgn_object_to_yaml_append(node, buf, len, cap, indent);
        default:
            return false;
    }
}

cxgn_document* cxgn_document_new(const char* source_name) {
    cxgn_document* doc = (cxgn_document*)calloc(1, sizeof(*doc));
    if (!doc) return NULL;

    if (source_name) {
        doc->source_name = cxgn_strdup(source_name);
        if (!doc->source_name) {
            free(doc);
            return NULL;
        }
    }

    return doc;
}

void cxgn_document_free(cxgn_document* doc) {
    if (!doc) return;
    cxgn_node_free(doc->root);
    free(doc->source_name);
    free(doc);
}

bool cxgn_document_set_root(cxgn_document* doc, cxgn_node* root) {
    if (!doc) return false;
    cxgn_node_free(doc->root);
    doc->root = root;
    return true;
}

cxgn_node* cxgn_node_new(cxgn_node_type type) {
    cxgn_node* node = (cxgn_node*)calloc(1, sizeof(*node));
    if (!node) return NULL;
    node->type = type;
    return node;
}

cxgn_node* cxgn_node_new_null(void) {
    return cxgn_node_new(CXGN_NODE_NULL);
}

cxgn_node* cxgn_node_new_bool(bool value) {
    cxgn_node* node = cxgn_node_new(CXGN_NODE_BOOL);
    if (!node) return NULL;
    node->as.bool_value = value;
    return node;
}

cxgn_node* cxgn_node_new_integer(long long value) {
    cxgn_node* node = cxgn_node_new(CXGN_NODE_INTEGER);
    char buf[64];
    int written;
    if (!node) return NULL;
    node->as.int_value = value;
    written = snprintf(buf, sizeof(buf), "%lld", value);
    if (written > 0) {
        if (!cxgn_node_set_raw_scalar_text(node, buf, (size_t)written)) {
            cxgn_node_free(node);
            return NULL;
        }
    }
    return node;
}

cxgn_node* cxgn_node_new_float(double value) {
    cxgn_node* node = cxgn_node_new(CXGN_NODE_FLOAT);
    char buf[64];
    int written;
    if (!node) return NULL;
    node->as.float_value = value;
    written = snprintf(buf, sizeof(buf), "%.17g", value);
    if (written > 0) {
        if (!cxgn_node_set_raw_scalar_text(node, buf, (size_t)written)) {
            cxgn_node_free(node);
            return NULL;
        }
    }
    return node;
}

cxgn_node* cxgn_node_new_string(const char* data, size_t len) {
    cxgn_node* node = cxgn_node_new(CXGN_NODE_STRING);
    if (!node) return NULL;

    node->as.string.data = cxgn_strndup(data ? data : "", len);
    if (!node->as.string.data) {
        free(node);
        return NULL;
    }
    node->as.string.len = len;
    return node;
}

cxgn_node* cxgn_node_new_array(void) {
    return cxgn_node_new(CXGN_NODE_ARRAY);
}

cxgn_node* cxgn_node_new_object(void) {
    return cxgn_node_new(CXGN_NODE_OBJECT);
}

void cxgn_node_set_location(cxgn_node* node, size_t line, size_t column) {
    if (!node) return;
    node->line = line;
    node->column = column;
}

bool cxgn_node_set_raw_scalar_text(cxgn_node* node, const char* text, size_t len) {
    if (!node) return false;

    char* copy = cxgn_strndup(text ? text : "", len);
    if (!copy) return false;

    free(node->raw_scalar_text);
    node->raw_scalar_text = copy;
    node->raw_scalar_len = len;
    return true;
}

void cxgn_node_free(cxgn_node* node) {
    if (!node) return;

    free(node->raw_scalar_text);

    switch (node->type) {
        case CXGN_NODE_STRING:
            free(node->as.string.data);
            break;
        case CXGN_NODE_ARRAY:
            for (size_t i = 0; i < node->as.array.count; i++) {
                cxgn_node_free(node->as.array.items[i]);
            }
            free(node->as.array.items);
            break;
        case CXGN_NODE_OBJECT:
            for (size_t i = 0; i < node->as.object.count; i++) {
                free(node->as.object.entries[i].key);
                cxgn_node_free(node->as.object.entries[i].value);
            }
            free(node->as.object.entries);
            break;
        default:
            break;
    }

    free(node);
}

bool cxgn_node_array_append(cxgn_node* array_node, cxgn_node* item) {
    if (!array_node || array_node->type != CXGN_NODE_ARRAY || !item) return false;

    size_t next_count = array_node->as.array.count + 1;
    cxgn_node** next_items =
        (cxgn_node**)realloc(array_node->as.array.items, next_count * sizeof(*next_items));
    if (!next_items) return false;

    array_node->as.array.items = next_items;
    array_node->as.array.items[array_node->as.array.count] = item;
    array_node->as.array.count = next_count;
    return true;
}

bool cxgn_node_object_append(cxgn_node* object_node,
                             const char* key,
                             cxgn_node* value,
                             size_t line,
                             size_t column) {
    if (!object_node || object_node->type != CXGN_NODE_OBJECT || !key || !value) return false;

    char* key_copy = cxgn_strdup(key);
    if (!key_copy) return false;

    size_t next_count = object_node->as.object.count + 1;
    cxgn_object_entry* next_entries =
        (cxgn_object_entry*)realloc(object_node->as.object.entries, next_count * sizeof(*next_entries));
    if (!next_entries) {
        free(key_copy);
        return false;
    }

    object_node->as.object.entries = next_entries;
    object_node->as.object.entries[object_node->as.object.count].key = key_copy;
    object_node->as.object.entries[object_node->as.object.count].value = value;
    object_node->as.object.entries[object_node->as.object.count].line = line;
    object_node->as.object.entries[object_node->as.object.count].column = column;
    object_node->as.object.count = next_count;
    return true;
}

const cxgn_node* cxgn_document_get_root(const cxgn_document* doc) {
    return doc ? doc->root : NULL;
}

const char* cxgn_document_get_source_name(const cxgn_document* doc) {
    return doc ? doc->source_name : NULL;
}

cxgn_node_type cxgn_node_get_type(const cxgn_node* node) {
    return node ? node->type : CXGN_NODE_NULL;
}

size_t cxgn_node_get_line(const cxgn_node* node) {
    return node ? node->line : 0u;
}

size_t cxgn_node_get_column(const cxgn_node* node) {
    return node ? node->column : 0u;
}

const char* cxgn_node_get_raw_scalar_text(const cxgn_node* node, size_t* len) {
    if (len) *len = node ? node->raw_scalar_len : 0u;
    return node ? node->raw_scalar_text : NULL;
}

bool cxgn_node_get_bool(const cxgn_node* node, bool* out_value) {
    if (!node || node->type != CXGN_NODE_BOOL || !out_value) return false;
    *out_value = node->as.bool_value;
    return true;
}

bool cxgn_node_get_integer(const cxgn_node* node, long long* out_value) {
    if (!node || node->type != CXGN_NODE_INTEGER || !out_value) return false;
    *out_value = node->as.int_value;
    return true;
}

bool cxgn_node_get_float(const cxgn_node* node, double* out_value) {
    if (!node || node->type != CXGN_NODE_FLOAT || !out_value) return false;
    *out_value = node->as.float_value;
    return true;
}

const char* cxgn_node_get_string(const cxgn_node* node, size_t* len) {
    if (!node || node->type != CXGN_NODE_STRING) {
        if (len) *len = 0u;
        return NULL;
    }
    if (len) *len = node->as.string.len;
    return node->as.string.data;
}

size_t cxgn_node_array_count(const cxgn_node* node) {
    return (node && node->type == CXGN_NODE_ARRAY) ? node->as.array.count : 0u;
}

const cxgn_node* cxgn_node_array_at(const cxgn_node* node, size_t index) {
    if (!node || node->type != CXGN_NODE_ARRAY || index >= node->as.array.count) return NULL;
    return node->as.array.items[index];
}

size_t cxgn_node_object_count(const cxgn_node* node) {
    return (node && node->type == CXGN_NODE_OBJECT) ? node->as.object.count : 0u;
}

const char* cxgn_node_object_key_at(const cxgn_node* node, size_t index) {
    if (!node || node->type != CXGN_NODE_OBJECT || index >= node->as.object.count) return NULL;
    return node->as.object.entries[index].key;
}

const cxgn_node* cxgn_node_object_value_at(const cxgn_node* node, size_t index) {
    if (!node || node->type != CXGN_NODE_OBJECT || index >= node->as.object.count) return NULL;
    return node->as.object.entries[index].value;
}

const cxgn_node* cxgn_node_object_find(const cxgn_node* node, const char* key, size_t ordinal) {
    size_t seen = 0u;
    size_t i;

    if (!node || node->type != CXGN_NODE_OBJECT || !key) return NULL;
    for (i = 0u; i < node->as.object.count; ++i) {
        const cxgn_object_entry* entry = &node->as.object.entries[i];
        if (entry->key && strcmp(entry->key, key) == 0) {
            if (seen == ordinal) return entry->value;
            ++seen;
        }
    }
    return NULL;
}

static cxgn_node* cxgn_node_clone_internal(const cxgn_node* node) {
    cxgn_node* out;
    size_t i;

    if (!node) return NULL;
    out = cxgn_node_new(node->type);
    if (!out) return NULL;
    out->line = node->line;
    out->column = node->column;
    if (node->raw_scalar_text &&
        !cxgn_node_set_raw_scalar_text(out, node->raw_scalar_text, node->raw_scalar_len)) {
        cxgn_node_free(out);
        return NULL;
    }

    switch (node->type) {
        case CXGN_NODE_BOOL:
            out->as.bool_value = node->as.bool_value;
            break;
        case CXGN_NODE_INTEGER:
            out->as.int_value = node->as.int_value;
            break;
        case CXGN_NODE_FLOAT:
            out->as.float_value = node->as.float_value;
            break;
        case CXGN_NODE_STRING:
            free(out->as.string.data);
            out->as.string.data = cxgn_strndup(node->as.string.data ? node->as.string.data : "",
                                               node->as.string.len);
            if (!out->as.string.data) {
                cxgn_node_free(out);
                return NULL;
            }
            out->as.string.len = node->as.string.len;
            break;
        case CXGN_NODE_ARRAY:
            for (i = 0u; i < node->as.array.count; ++i) {
                cxgn_node* item = cxgn_node_clone_internal(node->as.array.items[i]);
                if (!item || !cxgn_node_array_append(out, item)) {
                    cxgn_node_free(item);
                    cxgn_node_free(out);
                    return NULL;
                }
            }
            break;
        case CXGN_NODE_OBJECT:
            for (i = 0u; i < node->as.object.count; ++i) {
                const cxgn_object_entry* entry = &node->as.object.entries[i];
                cxgn_node* value = cxgn_node_clone_internal(entry->value);
                if (!value || !cxgn_node_object_append(out, entry->key, value, entry->line, entry->column)) {
                    cxgn_node_free(value);
                    cxgn_node_free(out);
                    return NULL;
                }
            }
            break;
        case CXGN_NODE_NULL:
        default:
            break;
    }
    return out;
}

static cxgn_node* cxgn_node_merge_internal(const cxgn_node* base_node, const cxgn_node* overlay_node) {
    cxgn_node* out;
    size_t i;

    if (!base_node) return cxgn_node_clone_internal(overlay_node);
    if (!overlay_node) return cxgn_node_clone_internal(base_node);

    if (base_node->type != CXGN_NODE_OBJECT || overlay_node->type != CXGN_NODE_OBJECT) {
        return cxgn_node_clone_internal(overlay_node);
    }

    out = cxgn_node_clone_internal(base_node);
    if (!out) return NULL;

    for (i = 0u; i < overlay_node->as.object.count; ++i) {
        const cxgn_object_entry* overlay_entry = &overlay_node->as.object.entries[i];
        size_t j;
        int replaced = 0;

        for (j = 0u; j < out->as.object.count; ++j) {
            cxgn_object_entry* out_entry = &out->as.object.entries[j];
            if (out_entry->key && overlay_entry->key &&
                strcmp(out_entry->key, overlay_entry->key) == 0) {
                cxgn_node* merged_value =
                    cxgn_node_merge_internal(out_entry->value, overlay_entry->value);
                if (!merged_value) {
                    cxgn_node_free(out);
                    return NULL;
                }
                cxgn_node_free(out_entry->value);
                out_entry->value = merged_value;
                out_entry->line = overlay_entry->line;
                out_entry->column = overlay_entry->column;
                replaced = 1;
                break;
            }
        }

        if (!replaced) {
            cxgn_node* cloned_value = cxgn_node_clone_internal(overlay_entry->value);
            if (!cloned_value ||
                !cxgn_node_object_append(
                    out, overlay_entry->key, cloned_value, overlay_entry->line, overlay_entry->column)) {
                cxgn_node_free(cloned_value);
                cxgn_node_free(out);
                return NULL;
            }
        }
    }

    return out;
}

cxgn_document* cxgn_document_clone(const cxgn_document* doc) {
    cxgn_document* out;
    cxgn_node* root_clone = NULL;

    if (!doc) return NULL;
    out = cxgn_document_new(doc->source_name);
    if (!out) return NULL;
    if (doc->root) {
        root_clone = cxgn_node_clone_internal(doc->root);
        if (!root_clone || !cxgn_document_set_root(out, root_clone)) {
            cxgn_node_free(root_clone);
            cxgn_document_free(out);
            return NULL;
        }
    }
    return out;
}

cxgn_document* cxgn_document_merge(const cxgn_document* base_doc, const cxgn_document* overlay_doc) {
    cxgn_document* out;
    cxgn_node* merged_root;
    const char* source_name;

    if (!base_doc && !overlay_doc) return NULL;
    if (!base_doc) return cxgn_document_clone(overlay_doc);
    if (!overlay_doc) return cxgn_document_clone(base_doc);

    source_name = overlay_doc->source_name ? overlay_doc->source_name : base_doc->source_name;
    out = cxgn_document_new(source_name);
    if (!out) return NULL;

    merged_root = cxgn_node_merge_internal(base_doc->root, overlay_doc->root);
    if (base_doc->root || overlay_doc->root) {
        if (!merged_root || !cxgn_document_set_root(out, merged_root)) {
            cxgn_node_free(merged_root);
            cxgn_document_free(out);
            return NULL;
        }
    }
    return out;
}

cxgn_document* cxgn_document_merge_many(const cxgn_document* const* docs, size_t count) {
    cxgn_document* merged = NULL;
    size_t i;

    if (!docs || count == 0u) return NULL;

    for (i = 0u; i < count; ++i) {
        const cxgn_document* next = docs[i];
        cxgn_document* updated;

        if (!next) continue;
        if (!merged) {
            merged = cxgn_document_clone(next);
            if (!merged) return NULL;
            continue;
        }

        updated = cxgn_document_merge(merged, next);
        cxgn_document_free(merged);
        if (!updated) return NULL;
        merged = updated;
    }

    return merged;
}

char* cxgn_document_to_yaml_text(const cxgn_document* doc) {
    char* buf = NULL;
    size_t len = 0u;
    size_t cap = 0u;
    if (!doc || !doc->root) return NULL;
    if (!cxgn_node_to_yaml_append(doc->root, &buf, &len, &cap, 0)) {
        free(buf);
        return NULL;
    }
    return buf;
}
