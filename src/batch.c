/**
 * @file batch.c
 * @brief Batch generation: multiple YAML files to a single combined .all.gen.h
 *
 * Each entry is generated with a unique symbol prefix so all static
 * variables coexist in one translation unit without collision.
 * A keyed map registry is appended at the end of the combined output.
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

void cxgn_batch_options_init(cxgn_batch_options* options) {
    if (!options) return;
    options->map_root = NULL;
    options->map_name = NULL;
    options->map_type = NULL;
    options->continue_on_error = false;
}

static void cxgn_batch_entry_clear(cxgn_batch_entry_result* entry) {
    if (!entry) return;
    free(entry->yaml_path);
    free(entry->key);
    free(entry->identifier);
    cxgn_output_free(entry->output);
    cxgn_error_clear(&entry->error);
    memset(entry, 0, sizeof(*entry));
}

void cxgn_batch_result_clear(cxgn_batch_result* result) {
    if (!result) return;
    cxgn_output_free(result->combined_output);
    for (size_t i = 0; i < result->entry_count; i++) {
        cxgn_batch_entry_clear(&result->entries[i]);
    }
    free(result->entries);
    memset(result, 0, sizeof(*result));
}

static bool cxgn_batch_error_clone(cxgn_error* dst, const cxgn_error* src) {
    if (!dst) return false;
    cxgn_error_init(dst);
    if (!src || src->code == CXGN_OK) return true;

    dst->code = src->code;
    dst->line = src->line;
    dst->column = src->column;

    if (src->needs_free) {
        if (src->message) {
            dst->message = cxgn_strdup(src->message);
            if (!dst->message) goto oom;
        }
        if (src->path) {
            dst->path = cxgn_strdup(src->path);
            if (!dst->path) goto oom;
        }
        dst->needs_free = (dst->message != NULL) || (dst->path != NULL);
    } else {
        dst->message = src->message;
        dst->path = src->path;
    }

    return true;

oom:
    cxgn_error_clear(dst);
    return false;
}

static bool cxgn_batch_entry_set_error(cxgn_batch_entry_result* entry, cxgn_error* err) {
    if (!entry || !err) return false;
    cxgn_error_clear(&entry->error);
    entry->error = *err;
    cxgn_error_init(err);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: path deduplication / append
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool cxgn_batch_push(cxgn_batch* batch, const char* path) {
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

static int cxgn_batch_cmp_path_str(const void* a, const void* b) {
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
    if (!cxgn_batch_push(batch, yaml_path)) {
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
        if (!cxgn_batch_push(batch, pl.paths[i])) {
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

static bool cxgn_batch_output_append(cxgn_output* out, const char* s) {
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

static bool cxgn_batch_output_appendf(cxgn_output* out, const char* fmt, ...) {
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

    const bool ok = cxgn_batch_output_append(out, buf);
    free(buf);
    return ok;
}

static cxgn_output* cxgn_batch_output_new(void) {
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
static char* cxgn_strip_intermediate_extensions(const char* stem, size_t stem_len) {
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

static bool cxgn_path_under_root(const char* path, const char* root, const char** rel_out) {
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

static char* cxgn_derive_key(const char* yaml_path, const char* map_root, cxgn_error* err) {
    const char* path = yaml_path;

    if (map_root && map_root[0]) {
        if (!cxgn_path_under_root(path, map_root, &path)) {
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
    return cxgn_strip_intermediate_extensions(path, len);
}

/**
 * @brief Convert a map key to a valid C identifier.
 *
 * Replaces non-alphanumeric characters with '_'.
 * Prepends '_' if the first character is a digit.
 */
static char* cxgn_key_to_ident(const char* key) {
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

bool cxgn_batch_generate_detailed(cxgn_batch* batch,
                                  const char* header_path,
                                  const cxgn_batch_options* options,
                                  cxgn_batch_result* result,
                                  cxgn_error* err) {
    cxgn_error_init(err);
    if (result) cxgn_batch_result_clear(result);

    if (!batch || !header_path || !result) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return false;
    }
    if (batch->count == 0) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Batch is empty");
        return false;
    }

    cxgn_batch_options effective_options;
    cxgn_batch_options_init(&effective_options);
    if (options) effective_options = *options;

    const char* map_root = effective_options.map_root;
    const char* map_name = effective_options.map_name ? effective_options.map_name
                                                      : "config";
    const char* map_type = effective_options.map_type ? effective_options.map_type
                                                      : "cxgn_map_entry_t";
    char* map_registry_type = NULL;
    const bool continue_on_error = effective_options.continue_on_error;

    /* Determine root struct name from the parsed header */
    const cxgn_struct_parser* parser = batch->gen->parser;
    size_t struct_count = cxgn_struct_parser_get_struct_count(parser);
    if (struct_count == 0) {
        cxgn_error_set(err, CXGN_ERR_UNKNOWN_STRUCT, "No struct found in header");
        return false;
    }
    const cxgn_struct_info* root_struct = NULL;
    if (batch->gen->root_struct_name && batch->gen->root_struct_name[0] != '\0') {
        root_struct = cxgn_struct_parser_find_struct(parser, batch->gen->root_struct_name);
        if (!root_struct) {
            cxgn_error_set(err, CXGN_ERR_UNKNOWN_STRUCT, "Requested root struct not found in header");
            return false;
        }
    } else {
        root_struct = cxgn_struct_parser_get_struct(parser, struct_count - 1);
    }
    const char* struct_name = cxgn_struct_get_name(root_struct);
    {
        const int needed = snprintf(NULL, 0, "%s_registry_t", map_name);
        if (needed < 0) {
            cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }
        map_registry_type = (char*)malloc((size_t)needed + 1u);
        if (!map_registry_type) {
            cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            return false;
        }
        snprintf(map_registry_type, (size_t)needed + 1u, "%s_registry_t", map_name);
    }
    if (!map_registry_type) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    char** ordered_paths = (char**)calloc(batch->count, sizeof(char*));
    if (!ordered_paths) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }
    for (size_t i = 0; i < batch->count; i++) ordered_paths[i] = batch->yaml_paths[i];
    qsort(ordered_paths, batch->count, sizeof(char*), cxgn_batch_cmp_path_str);

    result->entries = (cxgn_batch_entry_result*)calloc(batch->count, sizeof(*result->entries));
    if (!result->entries) {
        free(ordered_paths);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }
    result->entry_count = batch->count;

    char* saved_helpers = batch->gen->helpers_header;
    if (saved_helpers) batch->gen->helpers_header = NULL;

    for (size_t i = 0; i < batch->count; i++) {
        cxgn_batch_entry_result* entry = &result->entries[i];
        cxgn_error entry_err = {0};

        entry->yaml_path = cxgn_strdup(ordered_paths[i]);
        if (!entry->yaml_path) {
            cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            goto fail;
        }

        entry->key = cxgn_derive_key(ordered_paths[i], map_root, &entry_err);
        if (!entry->key) {
            result->failure_count++;
            cxgn_batch_entry_set_error(entry, &entry_err);
            if (!continue_on_error) {
                if (!cxgn_batch_error_clone(err, &entry->error)) goto fail;
                goto fail;
            }
            continue;
        }

        {
            char* base_identifier = cxgn_key_to_ident(entry->key);
            int needed;
            if (!base_identifier) {
                cxgn_error_set(&entry_err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
                result->failure_count++;
                cxgn_batch_entry_set_error(entry, &entry_err);
                if (!continue_on_error) {
                    if (!cxgn_batch_error_clone(err, &entry->error)) goto fail;
                    goto fail;
                }
                continue;
            }
            needed = snprintf(NULL, 0, "%s", base_identifier);
            if (needed < 0) {
                free(base_identifier);
                cxgn_error_set(&entry_err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
                result->failure_count++;
                cxgn_batch_entry_set_error(entry, &entry_err);
                if (!continue_on_error) {
                    if (!cxgn_batch_error_clone(err, &entry->error)) goto fail;
                    goto fail;
                }
                continue;
            }
            entry->identifier = (char*)malloc((size_t)needed + 1u);
            if (!entry->identifier) {
                free(base_identifier);
                cxgn_error_set(&entry_err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
                result->failure_count++;
                cxgn_batch_entry_set_error(entry, &entry_err);
                if (!continue_on_error) {
                    if (!cxgn_batch_error_clone(err, &entry->error)) goto fail;
                    goto fail;
                }
                continue;
            }
            snprintf(entry->identifier, (size_t)needed + 1u, "%s", base_identifier);
            free(base_identifier);
        }

        for (size_t j = 0; j < i; j++) {
            const cxgn_batch_entry_result* prev = &result->entries[j];
            if (!prev->output || !prev->key) continue;
            if (strcmp(entry->key, prev->key) == 0) {
                fprintf(stderr,
                        "cxgn: key collision \"%s\" -- %s and %s. "
                        "Use --map-root to derive unique keys from directory structure.\n",
                        entry->key, prev->yaml_path, entry->yaml_path);
                cxgn_error_set(&entry_err, CXGN_ERR_PARSE_ERROR,
                               "Key collision: use --map-root to disambiguate");
                result->failure_count++;
                cxgn_batch_entry_set_error(entry, &entry_err);
                if (!continue_on_error) {
                    if (!cxgn_batch_error_clone(err, &entry->error)) goto fail;
                    goto fail;
                }
                goto next_entry;
            }
        }

        size_t pfx_len = strlen(entry->identifier) + 2;
        char* pfx = (char*)malloc(pfx_len);
        if (!pfx) {
            cxgn_error_set(&entry_err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
            result->failure_count++;
            cxgn_batch_entry_set_error(entry, &entry_err);
            if (!continue_on_error) {
                if (!cxgn_batch_error_clone(err, &entry->error)) goto fail;
                goto fail;
            }
            continue;
        }
        snprintf(pfx, pfx_len, "%s_", entry->identifier);
        cxgn_generator_set_symbol_prefix(batch->gen, pfx);
        free(pfx);

        entry->output = cxgn_generate(batch->gen, ordered_paths[i], header_path, &entry_err);
        cxgn_generator_set_symbol_prefix(batch->gen, NULL);
        if (!entry->output) {
            result->failure_count++;
            cxgn_batch_entry_set_error(entry, &entry_err);
            if (!continue_on_error) {
                if (!cxgn_batch_error_clone(err, &entry->error)) goto fail;
                goto fail;
            }
            continue;
        }

        result->success_count++;
next_entry:
        ;
    }

    batch->gen->helpers_header = saved_helpers;

    if (result->success_count == 0) {
        if (result->failure_count > 0) {
            for (size_t i = 0; i < result->entry_count; i++) {
                if (result->entries[i].error.code != CXGN_OK) {
                    cxgn_batch_error_clone(err, &result->entries[i].error);
                    break;
                }
            }
        } else {
            cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "No batch entries generated successfully");
        }
        free(ordered_paths);
        return false;
    }

    result->combined_output = cxgn_batch_output_new();
    if (!result->combined_output) {
        free(ordered_paths);
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    if (saved_helpers) {
        cxgn_batch_output_appendf(result->combined_output, "#include <%s>\n\n", saved_helpers);
    }
    for (size_t i = 0; i < result->entry_count; i++) {
        const cxgn_batch_entry_result* entry = &result->entries[i];
        if (!entry->output) continue;
        cxgn_batch_output_appendf(result->combined_output, "/* -- Entry: %s -- Source: %s */\n",
                     entry->key, entry->yaml_path);
        cxgn_batch_output_append(result->combined_output, cxgn_output_get_code(entry->output));
        cxgn_batch_output_append(result->combined_output, "\n");
    }

    cxgn_batch_output_append(result->combined_output, "\n/* == Config registry == */\n");
    cxgn_batch_output_appendf(result->combined_output, "typedef struct {\n");
    cxgn_batch_output_appendf(result->combined_output, "    const char* key;\n");
    cxgn_batch_output_appendf(result->combined_output, "    const %s* config;\n", struct_name);
    cxgn_batch_output_appendf(result->combined_output, "} %s;\n\n", map_type);
    cxgn_batch_output_appendf(result->combined_output, "typedef struct %s {\n", map_registry_type);
    cxgn_batch_output_appendf(result->combined_output, "    const %s* entries;\n", map_type);
    cxgn_batch_output_appendf(result->combined_output, "    size_t count;\n");
    cxgn_batch_output_appendf(result->combined_output, "} %s;\n\n", map_registry_type);

    cxgn_batch_output_appendf(result->combined_output, "static const %s _%s_entries[] = {\n", map_type, map_name);
    for (size_t i = 0; i < result->entry_count; i++) {
        const cxgn_batch_entry_result* entry = &result->entries[i];
        if (!entry->output) continue;
        cxgn_batch_output_appendf(result->combined_output, "    {\"%s\", &%s_config},\n",
                     entry->key, entry->identifier);
    }
    cxgn_batch_output_append(result->combined_output, "};\n\n");
    cxgn_batch_output_appendf(result->combined_output, "static const %s %s = {\n", map_registry_type, map_name);
    cxgn_batch_output_appendf(result->combined_output, "    .entries = _%s_entries,\n", map_name);
    cxgn_batch_output_appendf(result->combined_output, "    .count = %zu,\n", result->success_count);
    cxgn_batch_output_append(result->combined_output, "};\n");

    free(map_registry_type);
    free(ordered_paths);
    return true;

fail:
    batch->gen->helpers_header = saved_helpers;
    free(map_registry_type);
    free(ordered_paths);
    if (err && err->code == CXGN_OK) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
    }
    return false;
}

cxgn_output* cxgn_batch_generate(cxgn_batch* batch,
                                 const char* header_path,
                                 const cxgn_batch_options* options,
                                 cxgn_error* err) {
    cxgn_batch_result result = {0};
    if (!cxgn_batch_generate_detailed(batch, header_path, options, &result, err)) {
        cxgn_batch_result_clear(&result);
        return NULL;
    }

    cxgn_output* combined = result.combined_output;
    result.combined_output = NULL;
    cxgn_batch_result_clear(&result);
    return combined;
}
