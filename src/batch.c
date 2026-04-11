/**
 * @file batch.c
 * @brief Batch generation: multiple YAML files to a single combined .all.gen.h
 *
 * Each entry is generated with a unique symbol prefix so all static
 * variables coexist in one translation unit without collision.
 * A keyed map array is appended at the end of the combined output.
 */

#define _POSIX_C_SOURCE 200809L

#include "internal.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Batch creation / destruction
 * ═══════════════════════════════════════════════════════════════════════════ */

cxgn_batch* cxgn_batch_new(cxgn_generator* gen) {
    if (!gen) return NULL;
    cxgn_batch* batch = (cxgn_batch*)calloc(1, sizeof(*batch));
    if (!batch) return NULL;
    batch->ref_count = 1;
    batch->gen = cxgn_generator_retain(gen);
    return batch;
}

cxgn_batch* cxgn_batch_retain(cxgn_batch* batch) {
    if (batch) batch->ref_count++;
    return batch;
}

void cxgn_batch_free(cxgn_batch* batch) {
    if (!batch) return;
    if (batch->ref_count > 1) {
        batch->ref_count--;
        return;
    }
    for (size_t i = 0; i < batch->count; i++) free(batch->yaml_paths[i]);
    free(batch->yaml_paths);
    cxgn_generator_free(batch->gen);
    free(batch);
}

size_t cxgn_batch_count(const cxgn_batch* batch) {
    return batch ? batch->count : 0;
}

const char* cxgn_batch_get_path(const cxgn_batch* batch, size_t index) {
    if (!batch || index >= batch->count) return NULL;
    return batch->yaml_paths[index];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: path deduplication / append
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool batch_push(cxgn_batch* batch, const char* path) {
    for (size_t i = 0; i < batch->count; i++) {
        if (strcmp(batch->yaml_paths[i], path) == 0) return true;
    }
    if (batch->count >= batch->capacity) {
        size_t new_cap = batch->capacity ? batch->capacity * 2 : 8;
        char** next = (char**)realloc(batch->yaml_paths, new_cap * sizeof(char*));
        if (!next) return false;
        batch->yaml_paths = next;
        batch->capacity = new_cap;
    }
    batch->yaml_paths[batch->count] = cxgn_strdup(path);
    if (!batch->yaml_paths[batch->count]) return false;
    batch->count++;
    return true;
}

static int cmp_path_str(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: add files
 * ═══════════════════════════════════════════════════════════════════════════ */

bool cxgn_batch_add_file(cxgn_batch* batch, const char* yaml_path, cxgn_error* err) {
    cxgn_error_init(err);
    if (!batch || !yaml_path) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return false;
    }
    struct stat st;
    if (stat(yaml_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        cxgn_error_set(err, CXGN_ERR_FILE_NOT_FOUND, "YAML file not found");
        return false;
    }
    if (!batch_push(batch, yaml_path)) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }
    return true;
}

bool cxgn_batch_add_glob(cxgn_batch* batch, const char* pattern, cxgn_error* err) {
    cxgn_error_init(err);
    if (!batch || !pattern) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return false;
    }

    path_list_t pl = {NULL, 0, 0};
    if (!cxgn_glob_expand(pattern, &pl, err)) return false;

    bool ok = true;
    for (size_t i = 0; i < pl.count; i++) {
        if (!batch_push(batch, pl.paths[i])) {
            cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            ok = false;
            break;
        }
    }
    for (size_t i = 0; i < pl.count; i++) free(pl.paths[i]);
    free(pl.paths);
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: output helpers (local to this file)
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool bout_append(cxgn_output* out, const char* s) {
    if (!out || !s) return false;
    size_t len = strlen(s);
    if (out->length + len + 1 > out->capacity) {
        size_t new_cap = out->capacity ? out->capacity * 2 : 8192;
        while (new_cap < out->length + len + 1) new_cap *= 2;
        char* next = (char*)realloc(out->code, new_cap);
        if (!next) return false;
        out->code = next;
        out->capacity = new_cap;
    }
    memcpy(out->code + out->length, s, len + 1);
    out->length += len;
    return true;
}

static bool bout_appendf(cxgn_output* out, const char* fmt, ...) {
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

    const bool ok = bout_append(out, buf);
    free(buf);
    return ok;
}

static cxgn_output* bout_new(void) {
    cxgn_output* o = (cxgn_output*)calloc(1, sizeof(*o));
    if (!o) return NULL;
    o->ref_count = 1;
    o->capacity = 8192;
    o->code = (char*)malloc(o->capacity);
    if (!o->code) { free(o); return NULL; }
    o->code[0] = '\0';
    return o;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: key / identifier helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Derive a map key from a YAML file path.
 *
 * Without map_root: returns the filename stem.
 * With map_root:    strips the root prefix and returns the relative path
 *                   without extension, using '/' as separator.
 */
/**
 * @brief Strip known generator-specific intermediate extensions from a stem.
 *
 * Handles compound filenames like "foo.manifest.yaml" or "foo.gen.h" by
 * repeatedly removing known suffixes (".manifest", ".gen", ".merged") from
 * the tail of the stem until none remain.
 * Returns a newly-allocated string; caller must free it.
 */
static char* strip_intermediate_extensions(const char* stem, size_t stem_len) {
    static const char* const known[] = {
        ".manifest", ".gen", ".merged", ".schema", NULL
    };
    char* s = cxgn_strndup(stem, stem_len);
    if (!s) return NULL;
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; known[i]; i++) {
            size_t klen = strlen(known[i]);
            size_t slen = strlen(s);
            if (slen > klen && strcmp(s + slen - klen, known[i]) == 0) {
                s[slen - klen] = '\0';
                changed = true;
            }
        }
    }
    return s;
}

static bool path_under_root(const char* path, const char* root, const char** rel_out) {
    size_t root_len = strlen(root);
    while (root_len > 0 && root[root_len - 1] == '/') root_len--;

    if (strncmp(path, root, root_len) != 0) return false;
    if (path[root_len] != '\0' && path[root_len] != '/') return false;

    const char* rel = path + root_len;
    while (*rel == '/') rel++;
    if (*rel == '\0') return false;

    if (rel_out) *rel_out = rel;
    return true;
}

static char* derive_key(const char* yaml_path, const char* map_root, cxgn_error* err) {
    const char* path = yaml_path;

    if (map_root && map_root[0]) {
        if (!path_under_root(path, map_root, &path)) {
            cxgn_error_set(err, CXGN_ERR_PARSE_ERROR,
                           "File is outside --map-root");
            return NULL;
        }
    }

    const char* last_slash = strrchr(path, '/');
    const char* dot        = strrchr(path, '.');
    size_t len;
    if (dot && (!last_slash || dot > last_slash)) {
        len = (size_t)(dot - path);
    } else {
        len = strlen(path);
    }

    /* Without map_root, use only the stem (filename portion). */
    if (!map_root || !map_root[0]) {
        if (last_slash) path = last_slash + 1;
        dot = strrchr(path, '.');
        len = dot ? (size_t)(dot - path) : strlen(path);
    }

    if (len == 0) return cxgn_strdup("_unnamed");

    /* Strip any intermediate known extensions (e.g. ".manifest", ".gen"). */
    return strip_intermediate_extensions(path, len);
}

/**
 * @brief Convert a map key to a valid C identifier.
 *
 * Replaces non-alphanumeric characters with '_'.
 * Prepends '_' if the first character is a digit.
 */
static char* key_to_ident(const char* key) {
    size_t len = strlen(key);
    char* ident = (char*)malloc(len + 2);
    if (!ident) return NULL;
    size_t w = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)key[i];
        ident[w++] = isalnum(c) ? (char)c : '_';
    }
    ident[w] = '\0';
    if (w > 0 && isdigit((unsigned char)ident[0])) {
        memmove(ident + 1, ident, w + 1);
        ident[0] = '_';
    }
    return ident;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: batch generate
 * ═══════════════════════════════════════════════════════════════════════════ */

cxgn_output* cxgn_batch_generate(cxgn_batch* batch,
                                  const char* header_path,
                                  const cxgn_batch_options* options,
                                  cxgn_error* err) {
    cxgn_error_init(err);

    if (!batch || !header_path) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return NULL;
    }
    if (batch->count == 0) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Batch is empty");
        return NULL;
    }

    const char* map_root = options ? options->map_root : NULL;
    const char* map_name = (options && options->map_name) ? options->map_name
                                                          : "cxgn_config_map";
    const char* map_type = (options && options->map_type) ? options->map_type
                                                          : "cxgn_map_entry_t";

    /* Determine root struct name from the parsed header */
    const cxgn_struct_parser* parser = batch->gen->parser;
    size_t struct_count = cxgn_struct_parser_get_struct_count(parser);
    if (struct_count == 0) {
        cxgn_error_set(err, CXGN_ERR_UNKNOWN_STRUCT, "No struct found in header");
        return NULL;
    }
    const cxgn_struct_info* root_struct =
        cxgn_struct_parser_get_struct(parser, struct_count - 1);
    const char* struct_name = cxgn_struct_get_name(root_struct);

    /* Derive keys and check for collisions */
    char** keys   = (char**)calloc(batch->count, sizeof(char*));
    char** idents = (char**)calloc(batch->count, sizeof(char*));
    if (!keys || !idents) {
        free(keys); free(idents);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return NULL;
    }

    char** ordered_paths = (char**)calloc(batch->count, sizeof(char*));
    if (!ordered_paths) goto oom_keys;
    for (size_t i = 0; i < batch->count; i++) ordered_paths[i] = batch->yaml_paths[i];
    qsort(ordered_paths, batch->count, sizeof(char*), cmp_path_str);

    for (size_t i = 0; i < batch->count; i++) {
        keys[i] = derive_key(ordered_paths[i], map_root, err);
        if (!keys[i]) goto oom_keys;
        idents[i] = key_to_ident(keys[i]);
        if (!idents[i]) goto oom_keys;

        for (size_t j = 0; j < i; j++) {
            if (strcmp(keys[i], keys[j]) == 0) {
                fprintf(stderr,
                        "cxgn: key collision \"%s\" -- %s and %s. "
                        "Use --map-root to derive unique keys from directory structure.\n",
                        keys[i], ordered_paths[j], ordered_paths[i]);
                cxgn_error_set(err, CXGN_ERR_PARSE_ERROR,
                               "Key collision: use --map-root to disambiguate");
                for (size_t k = 0; k <= i; k++) { free(keys[k]); free(idents[k]); }
                free(ordered_paths);
                free(keys); free(idents);
                return NULL;
            }
        }
    }

    /* Combined output buffer */
    cxgn_output* combined = bout_new();
    if (!combined) goto oom_keys;

    /* Emit helpers_header once (suppress per-entry repeat) */
    char* saved_helpers = batch->gen->helpers_header;
    if (saved_helpers) {
        bout_appendf(combined, "#include <%s>\n\n", saved_helpers);
        batch->gen->helpers_header = NULL;
    }

    /* Generate each entry */
    for (size_t i = 0; i < batch->count; i++) {
        /* Build symbol prefix: "{ident}_" */
        size_t pfx_len = strlen(idents[i]) + 2;
        char* pfx = (char*)malloc(pfx_len);
        if (!pfx) {
            batch->gen->helpers_header = saved_helpers;
            cxgn_output_free(combined);
            cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            for (size_t k = 0; k < batch->count; k++) { free(keys[k]); free(idents[k]); }
            free(ordered_paths);
            free(keys); free(idents);
            return NULL;
        }
        snprintf(pfx, pfx_len, "%s_", idents[i]);
        cxgn_generator_set_symbol_prefix(batch->gen, pfx);
        free(pfx);

        bout_appendf(combined, "/* -- Entry: %s -- Source: %s */\n",
                     keys[i], ordered_paths[i]);

        cxgn_output* entry = cxgn_generate(batch->gen, ordered_paths[i],
                                            header_path, err);
        cxgn_generator_set_symbol_prefix(batch->gen, NULL);

        if (!entry) {
            batch->gen->helpers_header = saved_helpers;
            cxgn_output_free(combined);
            for (size_t k = 0; k < batch->count; k++) { free(keys[k]); free(idents[k]); }
            free(ordered_paths);
            free(keys); free(idents);
            return NULL;
        }

        bout_append(combined, cxgn_output_get_code(entry));
        bout_append(combined, "\n");
        cxgn_output_free(entry);
    }

    batch->gen->helpers_header = saved_helpers;

    /* Emit map */
    bout_append(combined, "\n/* == Config map == */\n");
    bout_appendf(combined, "typedef struct {\n");
    bout_appendf(combined, "    const char* key;\n");
    bout_appendf(combined, "    const %s* config;\n", struct_name);
    bout_appendf(combined, "} %s;\n\n", map_type);
    bout_appendf(combined, "static const %s %s[] = {\n", map_type, map_name);
    for (size_t i = 0; i < batch->count; i++) {
        /* Variable name is "{ident}_config" (prefix is "{ident}_") */
        bout_appendf(combined, "    {\"%s\", &%s_config},\n", keys[i], idents[i]);
    }
    bout_append(combined, "};\n");
    bout_appendf(combined, "static const size_t %s_count = %zu;\n",
                 map_name, batch->count);

    free(ordered_paths);
    for (size_t i = 0; i < batch->count; i++) { free(keys[i]); free(idents[i]); }
    free(keys); free(idents);
    return combined;

oom_keys:
    if (err && err->code == CXGN_OK) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
    }
    for (size_t i = 0; i < batch->count; i++) {
        if (keys)   free(keys[i]);
        if (idents) free(idents[i]);
    }
    free(ordered_paths);
    free(keys); free(idents);
    return NULL;
}
