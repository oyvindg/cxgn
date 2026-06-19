/**
 * @file path.c
 * @brief YAML path tracking for error messages.
 *
 * Provides path tracking utilities for generating meaningful error
 * messages that show exactly where in the YAML structure an error occurred.
 */

#include "internal.h"
#include <stdio.h>

typedef struct {
    char** items;
    size_t count;
} cxgn_path_parts;

static void cxgn_free_path_parts(cxgn_path_parts* parts) {
    if (!parts || !parts->items) return;
    for (size_t i = 0; i < parts->count; i++) {
        free(parts->items[i]);
    }
    free(parts->items);
    parts->items = NULL;
    parts->count = 0;
}

static bool cxgn_append_path_part(cxgn_path_parts* parts, const char* start, size_t len) {
    char** new_items = (char**)realloc(parts->items, (parts->count + 1) * sizeof(char*));
    if (!new_items) return false;
    parts->items = new_items;
    parts->items[parts->count] = cxgn_strndup(start, len);
    if (!parts->items[parts->count]) return false;
    parts->count++;
    return true;
}

static bool cxgn_split_normalized_parts(const char* path, cxgn_path_parts* parts) {
    const char* p = path;

    parts->items = NULL;
    parts->count = 0;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        const char* segment = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - segment);

        if (len == 1 && segment[0] == '.') {
            continue;
        }

        if (len == 2 && segment[0] == '.' && segment[1] == '.') {
            if (parts->count > 0 && strcmp(parts->items[parts->count - 1], "..") != 0) {
                free(parts->items[--parts->count]);
                parts->items[parts->count] = NULL;
                continue;
            }
        }

        if (!cxgn_append_path_part(parts, segment, len)) {
            cxgn_free_path_parts(parts);
            return false;
        }
    }

    return true;
}

static char* cxgn_join_parts(const cxgn_path_parts* parts, size_t start_index) {
    size_t required = 1;

    for (size_t i = start_index; i < parts->count; i++) {
        required += strlen(parts->items[i]) + 1;
    }

    if (required == 1) {
        return cxgn_strdup(".");
    }

    char* result = (char*)malloc(required);
    if (!result) return NULL;

    size_t pos = 0;
    for (size_t i = start_index; i < parts->count; i++) {
        size_t len = strlen(parts->items[i]);
        memcpy(result + pos, parts->items[i], len);
        pos += len;
        if (i + 1 < parts->count) {
            result[pos++] = '/';
        }
    }
    result[pos] = '\0';
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Path Creation/Destruction
 * ═══════════════════════════════════════════════════════════════════════════ */

cxgn_path* cxgn_path_new(void) {
    cxgn_path* path = (cxgn_path*)malloc(sizeof(cxgn_path));
    if (path) {
        path->depth = 0;
        for (size_t i = 0; i < CXGN_MAX_PATH_DEPTH; i++) {
            path->segments[i].type = CXGN_PATH_KEY;
            path->segments[i].data.key = NULL;
        }
    }
    return path;
}

void cxgn_path_free(cxgn_path* path) {
    if (!path) return;

    /* Free all key strings */
    for (size_t i = 0; i < path->depth; i++) {
        if (path->segments[i].type == CXGN_PATH_KEY && path->segments[i].data.key) {
            free(path->segments[i].data.key);
        }
    }

    free(path);
}

char* cxgn_get_directory(const char* path) {
    const char* last_slash;

    if (!path || !*path) return cxgn_strdup(".");

    last_slash = strrchr(path, '/');
    if (!last_slash) return cxgn_strdup(".");
    if (last_slash == path) return cxgn_strndup(path, 1);
    return cxgn_strndup(path, (size_t)(last_slash - path));
}

char* cxgn_path_join(const char* dir, const char* file) {
    size_t dir_len;
    size_t file_len;
    char* result;

    if (!dir || !*dir) return cxgn_strdup(file ? file : "");
    if (!file || !*file) return cxgn_strdup(dir);

    dir_len = strlen(dir);
    file_len = strlen(file);
    result = (char*)malloc(dir_len + 1 + file_len + 1);
    if (!result) return NULL;

    memcpy(result, dir, dir_len);
    if (dir[dir_len - 1] == '/') {
        memcpy(result + dir_len, file, file_len + 1);
    } else {
        result[dir_len] = '/';
        memcpy(result + dir_len + 1, file, file_len + 1);
    }
    return result;
}

char* cxgn_path_relative_to_file(const char* from_path, const char* target_path) {
    char* from_dir;
    cxgn_path_parts from_parts;
    cxgn_path_parts target_parts;
    size_t common = 0;
    size_t required = 1;
    char* result;
    size_t pos = 0;

    if (!from_path || !target_path) return NULL;

    from_dir = cxgn_get_directory(from_path);
    if (!from_dir) return NULL;

    if (!cxgn_split_normalized_parts(from_dir, &from_parts)) {
        free(from_dir);
        return NULL;
    }
    free(from_dir);

    if (!cxgn_split_normalized_parts(target_path, &target_parts)) {
        cxgn_free_path_parts(&from_parts);
        return NULL;
    }

    while (common < from_parts.count &&
           common < target_parts.count &&
           strcmp(from_parts.items[common], target_parts.items[common]) == 0) {
        common++;
    }

    for (size_t i = common; i < from_parts.count; i++) {
        required += 3;
    }
    for (size_t i = common; i < target_parts.count; i++) {
        required += strlen(target_parts.items[i]) + 1;
    }

    if (required == 1) {
        result = cxgn_strdup(".");
        cxgn_free_path_parts(&from_parts);
        cxgn_free_path_parts(&target_parts);
        return result;
    }

    result = (char*)malloc(required);
    if (!result) {
        cxgn_free_path_parts(&from_parts);
        cxgn_free_path_parts(&target_parts);
        return NULL;
    }

    for (size_t i = common; i < from_parts.count; i++) {
        result[pos++] = '.';
        result[pos++] = '.';
        result[pos++] = '/';
    }

    for (size_t i = common; i < target_parts.count; i++) {
        size_t len = strlen(target_parts.items[i]);
        memcpy(result + pos, target_parts.items[i], len);
        pos += len;
        if (i + 1 < target_parts.count) {
            result[pos++] = '/';
        }
    }

    if (pos > 0 && result[pos - 1] == '/') {
        pos--;
    }
    result[pos] = '\0';

    cxgn_free_path_parts(&from_parts);
    cxgn_free_path_parts(&target_parts);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Path Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

void cxgn_path_push(cxgn_path* path, const char* key) {
    if (!path || !key) return;
    if (path->depth >= CXGN_MAX_PATH_DEPTH) return;

    path->segments[path->depth].type = CXGN_PATH_KEY;
    path->segments[path->depth].data.key = cxgn_strdup(key);
    path->depth++;
}

void cxgn_path_push_index(cxgn_path* path, size_t index) {
    if (!path) return;
    if (path->depth >= CXGN_MAX_PATH_DEPTH) return;

    path->segments[path->depth].type = CXGN_PATH_INDEX;
    path->segments[path->depth].data.index = index;
    path->depth++;
}

void cxgn_path_pop(cxgn_path* path) {
    if (!path || path->depth == 0) return;

    path->depth--;

    /* Free key if it was a key segment */
    if (path->segments[path->depth].type == CXGN_PATH_KEY) {
        free(path->segments[path->depth].data.key);
        path->segments[path->depth].data.key = NULL;
    }
}

char* cxgn_path_to_string(const cxgn_path* path) {
    if (!path) return cxgn_strdup("");

    /* Calculate required buffer size */
    size_t required = 1;  /* For null terminator */
    for (size_t i = 0; i < path->depth; i++) {
        if (path->segments[i].type == CXGN_PATH_KEY) {
            if (i > 0) required++;  /* For '.' separator */
            required += strlen(path->segments[i].data.key);
        } else {
            /* [index] - max 20 digits for size_t + brackets */
            required += 22;
        }
    }

    char* result = (char*)malloc(required);
    if (!result) return NULL;

    size_t pos = 0;
    for (size_t i = 0; i < path->depth; i++) {
        if (path->segments[i].type == CXGN_PATH_KEY) {
            if (i > 0) {
                result[pos++] = '.';
            }
            const char* key = path->segments[i].data.key;
            size_t key_len = strlen(key);
            memcpy(result + pos, key, key_len);
            pos += key_len;
        } else {
            pos += (size_t)sprintf(result + pos, "[%zu]", path->segments[i].data.index);
        }
    }

    result[pos] = '\0';
    return result;
}
