/**
 * @file glob.c
 * @brief Recursive glob file expansion for batch generation.
 *
 * Supports standard glob metacharacters (*, ?, [...]) and the
 * recursive wildcard ** (matches zero or more directory levels).
 * Uses only POSIX APIs: opendir/readdir/fnmatch/stat.
 */

#define _POSIX_C_SOURCE 200809L

#include "internal.h"
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: path_list_t helpers (type defined in internal.h)
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool pl_push(path_list_t* pl, const char* path) {
    if (pl->count >= pl->capacity) {
        size_t new_cap = pl->capacity ? pl->capacity * 2 : 16;
        char** next = (char**)realloc(pl->paths, new_cap * sizeof(char*));
        if (!next) return false;
        pl->paths = next;
        pl->capacity = new_cap;
    }
    pl->paths[pl->count] = cxgn_strdup(path);
    if (!pl->paths[pl->count]) return false;
    pl->count++;
    return true;
}

static int cmp_str(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static bool pl_contains(const path_list_t* pl, const char* path) {
    for (size_t i = 0; i < pl->count; i++) {
        if (strcmp(pl->paths[i], path) == 0) return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: pattern helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool has_glob_chars(const char* s) {
    for (; *s; s++) {
        if (*s == '*' || *s == '?' || *s == '[') return true;
    }
    return false;
}

/**
 * @brief Split a path by '/' into segments.
 *
 * An absolute path starting with '/' produces a leading empty string as the
 * first segment; the caller detects this and uses "/" as the base directory.
 *
 * @param pattern Input pattern string
 * @param out_count Number of segments returned
 * @return Heap-allocated array of heap-allocated strings, or NULL on OOM.
 *         Caller must call free_segments() on the result.
 */
static char** split_segments(const char* pattern, size_t* out_count) {
    size_t n = 1;
    for (const char* p = pattern; *p; p++) {
        if (*p == '/') n++;
    }

    char** segs = (char**)calloc(n, sizeof(char*));
    if (!segs) return NULL;

    size_t idx = 0;
    const char* start = pattern;
    for (const char* p = pattern; ; p++) {
        if (*p == '/' || *p == '\0') {
            size_t len = (size_t)(p - start);
            segs[idx] = cxgn_strndup(start, len);
            if (!segs[idx]) {
                for (size_t i = 0; i < idx; i++) free(segs[i]);
                free(segs);
                return NULL;
            }
            idx++;
            start = p + 1;
            if (*p == '\0') break;
        }
    }

    *out_count = n;
    return segs;
}

static void free_segments(char** segs, size_t n) {
    if (!segs) return;
    for (size_t i = 0; i < n; i++) free(segs[i]);
    free(segs);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: recursive collection
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Recursively collect files matching segs[si..nseg-1] under dir.
 *
 * segs[si] equal to "**" is the recursive wildcard: it matches zero or more
 * directory levels by trying the rest of the pattern in the current directory
 * and also descending into every subdirectory while keeping ** active.
 *
 * @param dir   Current directory to scan
 * @param segs  Pattern segments array
 * @param nseg  Total number of segments
 * @param si    Index of the segment to match at this level
 * @param out   Result accumulator
 */
static void collect(const char* dir, char** segs, size_t nseg, size_t si,
                    path_list_t* out) {
    if (si >= nseg) return;

    const char* seg = segs[si];
    bool is_last = (si + 1 == nseg);
    bool is_doublestar = (strcmp(seg, "**") == 0);

    if (is_doublestar) {
        /* Zero levels: try matching the rest of the pattern from this dir */
        if (!is_last) {
            collect(dir, segs, nseg, si + 1, out);
        }
        /* One or more levels: descend into each subdirectory */
        DIR* d = opendir(dir);
        if (!d) return;
        struct dirent* ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char child[PATH_MAX];
            snprintf(child, sizeof(child), "%s/%s", dir, ent->d_name);
            struct stat st;
            if (stat(child, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                /* Try rest of pattern from this subdir */
                if (!is_last) collect(child, segs, nseg, si + 1, out);
                /* Re-apply doublestar one level deeper */
                collect(child, segs, nseg, si, out);
            }
        }
        closedir(d);
    } else {
        /* Normal segment: enumerate entries matching this glob segment */
        DIR* d = opendir(dir);
        if (!d) return;
        struct dirent* ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            if (fnmatch(seg, ent->d_name, 0) != 0) continue;

            char child[PATH_MAX];
            snprintf(child, sizeof(child), "%s/%s", dir, ent->d_name);
            struct stat st;
            if (stat(child, &st) != 0) continue;

            if (is_last) {
                if (S_ISREG(st.st_mode) && !pl_contains(out, child)) {
                    pl_push(out, child);
                }
            } else {
                if (S_ISDIR(st.st_mode)) {
                    collect(child, segs, nseg, si + 1, out);
                }
            }
        }
        closedir(d);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Public: cxgn_glob_expand
 * ═══════════════════════════════════════════════════════════════════════════ */

bool cxgn_glob_expand(const char* pattern, path_list_t* out, cxgn_error* err) {
    if (!pattern || !out) {
        cxgn_error_set(err, CXGN_ERR_PARSE_ERROR, "Invalid arguments");
        return false;
    }

    /* Fast path: no glob chars — treat as literal path */
    if (!has_glob_chars(pattern) && !strstr(pattern, "**")) {
        struct stat st;
        if (stat(pattern, &st) == 0 && S_ISREG(st.st_mode)) {
            if (!pl_contains(out, pattern) && !pl_push(out, pattern)) {
                cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
                return false;
            }
        }
        return true;
    }

    size_t nseg = 0;
    char** segs = split_segments(pattern, &nseg);
    if (!segs) {
        cxgn_error_set(err, CXGN_ERR_OUT_OF_MEMORY, "Out of memory");
        return false;
    }

    /* Determine fixed base directory (leading segments without glob chars) */
    size_t first_glob = nseg;
    for (size_t i = 0; i < nseg; i++) {
        if (has_glob_chars(segs[i]) || strcmp(segs[i], "**") == 0) {
            first_glob = i;
            break;
        }
    }

    char base[PATH_MAX];
    if (first_glob == 0) {
        if (segs[0][0] == '\0') {
            /* Absolute path: empty first segment from leading '/' */
            base[0] = '/';
            base[1] = '\0';
            first_glob = 1;
        } else {
            base[0] = '.';
            base[1] = '\0';
        }
    } else {
        size_t pos = 0;
        if (segs[0][0] == '\0') {
            base[pos++] = '/';
            for (size_t i = 1; i < first_glob; i++) {
                if (i > 1) base[pos++] = '/';
                size_t len = strlen(segs[i]);
                memcpy(base + pos, segs[i], len);
                pos += len;
            }
        } else {
            for (size_t i = 0; i < first_glob; i++) {
                if (i > 0) base[pos++] = '/';
                size_t len = strlen(segs[i]);
                memcpy(base + pos, segs[i], len);
                pos += len;
            }
        }
        base[pos] = '\0';
    }

    size_t prev_count = out->count;
    collect(base, segs, nseg, first_glob, out);
    free_segments(segs, nseg);

    /* Sort the newly added entries for deterministic output */
    if (out->count > prev_count) {
        qsort(out->paths + prev_count, out->count - prev_count,
              sizeof(char*), cmp_str);
    }

    return true;
}
