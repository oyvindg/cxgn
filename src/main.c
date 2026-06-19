/**
 * @file main.c
 * @brief Cxgn CLI tool.
 *
 * Single-file mode (existing):
 *   cxgn --yaml config.yaml --header Config.h --output config.gen.h
 *
 * Batch mode (multiple --yaml or glob pattern):
 *   cxgn --yaml "strategies/\**.yaml"    --header Config.h --output all.gen.h
 *   cxgn --yaml a.yaml --yaml b.yaml   --header Config.h --output all.gen.h
 *
 * Batch options:
 *   --map-root <dir>    Strip this prefix from file paths when deriving map keys.
 *   --map-name <name>   C identifier for the emitted registry variable (default: config).
 *   --map-type <name>   C identifier for the map entry typedef (default: cxgn_map_entry_t).
 *   --strict            Upgrade validation warnings to errors.
 */

#include <cxgn/cxgn.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Local arg structure (superset of cxgn_cli_args)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* --yaml values (may be repeated) */
    const char** yaml_patterns;
    size_t        yaml_count;
    size_t        yaml_capacity;

    const char* plan_path;
    const char* header_path;
    const char* header_include;
    const char* output_path;
    const char* helpers_header;
    const char* root_struct;
    const char* map_root;
    const char* map_name;
    const char* map_type;
    bool        strict;
    bool        verbose;
} cli_args;

static int cxgn_cli_run_single(const cli_args* args,
                      cxgn_generator* gen,
                      cxgn_struct_parser* parser,
                      cxgn_string_utils* utils);
static int cxgn_cli_run_batch(const cli_args* args,
                     cxgn_generator* gen,
                     cxgn_struct_parser* parser,
                     cxgn_string_utils* utils);
static void cxgn_cli_diagnostic_sink(cxgn_diagnostic_level level,
                                const cxgn_error* diagnostic,
                                void* userdata);

static void cxgn_cli_args_free(cli_args* args) {
    free(args->yaml_patterns);
}

static bool cxgn_cli_args_add_yaml(cli_args* args, const char* val) {
    if (args->yaml_count >= args->yaml_capacity) {
        size_t new_cap = args->yaml_capacity ? args->yaml_capacity * 2 : 4;
        const char** next = (const char**)realloc(args->yaml_patterns,
                                                   new_cap * sizeof(const char*));
        if (!next) return false;
        args->yaml_patterns = next;
        args->yaml_capacity = new_cap;
    }
    args->yaml_patterns[args->yaml_count++] = val;
    return true;
}

static bool cxgn_cli_is_option_token(const char* arg) {
    return arg && arg[0] == '-' && arg[1] != '\0';
}

static char* cxgn_cli_duplicate_string(const char* value) {
    size_t len;
    char* copy;
    if (!value) return NULL;
    len = strlen(value);
    copy = (char*)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, value, len + 1);
    return copy;
}

static char* cxgn_cli_path_get_directory(const char* path) {
    const char* last_slash;
    if (!path || !*path) return cxgn_cli_duplicate_string(".");
    last_slash = strrchr(path, '/');
    if (!last_slash) return cxgn_cli_duplicate_string(".");
    if (last_slash == path) {
        char* root = (char*)malloc(2);
        if (!root) return NULL;
        root[0] = '/';
        root[1] = '\0';
        return root;
    }
    {
        size_t len = (size_t)(last_slash - path);
        char* dir = (char*)malloc(len + 1);
        if (!dir) return NULL;
        memcpy(dir, path, len);
        dir[len] = '\0';
        return dir;
    }
}

static char* cxgn_cli_path_join_local(const char* dir, const char* file) {
    size_t dir_len;
    size_t file_len;
    char* result;
    if (!dir || !*dir) return cxgn_cli_duplicate_string(file ? file : "");
    if (!file || !*file) return cxgn_cli_duplicate_string(dir);
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

static void cxgn_cli_print_usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s --yaml <file|pattern> --header <file> --output <file>\n"
            "          [--yaml <file|pattern>]...\n"
            "       %s --plan <file.yaml>\n"
            "          [--map-root <dir>] [--map-name <name>] [--map-type <name>]\n"
            "          [--helpers-header <path>] [--root-struct <name>] [--strict] [--verbose]\n"
            "\n"
            "Options:\n"
            "  --plan             YAML file containing a generic codegen: job list\n"
            "  --yaml, -y         YAML file or glob pattern (repeatable; can take multiple values)\n"
            "  --header, -h       C header with struct definitions\n"
            "  --header-include   Header path emitted in generated #include (default: relative to --output)\n"
            "  --output, -o       Output file path\n"
            "  --map-root         Directory prefix stripped from paths in batch keys\n"
            "  --map-name         C identifier for the map registry (batch; default: config)\n"
            "  --map-type         C identifier for the map entry typedef (batch; default: cxgn_map_entry_t)\n"
            "  --helpers-header   Emit #include <path> instead of inlining cxgn helper typedefs\n"
            "  --root-struct      Override the generated root struct when the header has multiple candidates\n"
            "  --strict           Treat validation warnings as errors\n"
            "  --verbose, -v      Enable verbose output\n"
            "  --help             Show this help\n",
            prog, prog);
}

static bool cxgn_cli_parse_args(int argc, char* argv[], cli_args* args) {
    memset(args, 0, sizeof(*args));

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];

#define REQUIRE_NEXT(flag) \
    do { if (i + 1 >= argc) { \
        fprintf(stderr, "Error: missing value for %s\n", flag); return false; \
    } } while (0)

        if (strcmp(a, "--yaml") == 0 || strcmp(a, "-y") == 0) {
            bool added = false;
            while (i + 1 < argc && !cxgn_cli_is_option_token(argv[i + 1])) {
                if (!cxgn_cli_args_add_yaml(args, argv[++i])) {
                    fprintf(stderr, "Error: out of memory\n"); return false;
                }
                added = true;
            }
            if (!added) {
                fprintf(stderr, "Error: missing value for --yaml\n");
                return false;
            }
        } else if (strcmp(a, "--plan") == 0) {
            REQUIRE_NEXT("--plan");
            args->plan_path = argv[++i];
        } else if (strcmp(a, "--header") == 0 || strcmp(a, "-h") == 0) {
            REQUIRE_NEXT("--header");
            args->header_path = argv[++i];
        } else if (strcmp(a, "--header-include") == 0) {
            REQUIRE_NEXT("--header-include");
            args->header_include = argv[++i];
        } else if (strcmp(a, "--output") == 0 || strcmp(a, "-o") == 0) {
            REQUIRE_NEXT("--output");
            args->output_path = argv[++i];
        } else if (strcmp(a, "--helpers-header") == 0) {
            REQUIRE_NEXT("--helpers-header");
            args->helpers_header = argv[++i];
        } else if (strcmp(a, "--root-struct") == 0) {
            REQUIRE_NEXT("--root-struct");
            args->root_struct = argv[++i];
        } else if (strcmp(a, "--map-root") == 0) {
            REQUIRE_NEXT("--map-root");
            args->map_root = argv[++i];
        } else if (strcmp(a, "--map-name") == 0) {
            REQUIRE_NEXT("--map-name");
            args->map_name = argv[++i];
        } else if (strcmp(a, "--map-type") == 0) {
            REQUIRE_NEXT("--map-type");
            args->map_type = argv[++i];
        } else if (strcmp(a, "--strict") == 0) {
            args->strict = true;
        } else if (strcmp(a, "--verbose") == 0 || strcmp(a, "-v") == 0) {
            args->verbose = true;
        } else if (strcmp(a, "--help") == 0) {
            cxgn_cli_print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Error: unknown argument: %s\n", a);
            return false;
        }
#undef REQUIRE_NEXT
    }

    if (args->plan_path) {
        if (args->yaml_count > 0 || args->header_path || args->output_path) {
            fprintf(stderr, "Error: --plan cannot be combined with --yaml/--header/--output\n");
            return false;
        }
        return true;
    }

    if (args->yaml_count == 0 || !args->header_path || !args->output_path) {
        fprintf(stderr, "Error: --yaml, --header and --output are required\n");
        return false;
    }
    return true;
}

/* Returns true when a string contains glob metacharacters or "**". */
static bool cxgn_cli_is_glob_pattern(const char* s) {
    if (strstr(s, "**")) return true;
    for (; *s; s++) {
        if (*s == '*' || *s == '?' || *s == '[') return true;
    }
    return false;
}

/* Returns true when batch mode should be used (multiple files or glob). */
static bool cxgn_cli_needs_batch(const cli_args* args) {
    if (args->yaml_count > 1) return true;
    return cxgn_cli_is_glob_pattern(args->yaml_patterns[0]);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Include guard from output path
 * ═══════════════════════════════════════════════════════════════════════════ */

static char* cxgn_cli_make_include_guard(const char* output_path) {
    size_t len = strlen(output_path);
    char* guard = (char*)malloc(len + 16);
    if (!guard) return NULL;
    size_t w = 0;
    guard[w++] = 'C';
    guard[w++] = 'X';
    guard[w++] = 'G';
    guard[w++] = 'N';
    guard[w++] = '_';
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)output_path[i];
        if (isalnum(c)) guard[w++] = (char)toupper(c);
        else guard[w++] = '_';
    }
    while (w > 0 && guard[w - 1] == '_') w--;
    if (!(w >= 2 && guard[w - 2] == '_' && guard[w - 1] == 'H')) {
        guard[w++] = '_';
        guard[w++] = 'H';
    }
    guard[w] = '\0';
    return guard;
}

static int cxgn_cli_cmp_path_str(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static const cxgn_node* cxgn_cli_find_object_key(const cxgn_node* node, const char* key) {
    return node ? cxgn_node_object_find(node, key, 0) : NULL;
}

static const char* cxgn_cli_node_string_value(const cxgn_node* node) {
    size_t len = 0;
    const char* value;
    if (!node || cxgn_node_get_type(node) != CXGN_NODE_STRING) return NULL;
    value = cxgn_node_get_string(node, &len);
    return (value && len > 0) ? value : value;
}

static bool cxgn_cli_node_bool_value(const cxgn_node* node, bool default_value) {
    bool value = false;
    if (!node) return default_value;
    if (!cxgn_node_get_bool(node, &value)) return default_value;
    return value;
}

static bool cxgn_cli_args_set_yaml_copy(cli_args* args, const char* value) {
    char* copy;
    if (!args || !value) return false;
    copy = cxgn_cli_duplicate_string(value);
    if (!copy) return false;
    if (!cxgn_cli_args_add_yaml(args, copy)) {
        free(copy);
        return false;
    }
    return true;
}

static void cxgn_cli_args_release_owned_yaml(cli_args* args) {
    if (!args) return;
    for (size_t i = 0; i < args->yaml_count; i++) {
        free((char*)args->yaml_patterns[i]);
    }
}

static int cxgn_cli_execute_job(const cli_args* args) {
    int result;
    cxgn_error err = {0};
    cxgn_string_utils* utils;
    cxgn_struct_parser* parser;
    cxgn_generator* gen;
    cxgn_validation_options validation;

    if (args->verbose)
        printf("Parsing header: %s\n", args->header_path);

    utils = cxgn_string_utils_new();
    if (!utils) {
        fprintf(stderr, "Error: out of memory\n");
        return 1;
    }

    parser = cxgn_struct_parser_new(utils);
    if (!parser) {
        fprintf(stderr, "Error: out of memory\n");
        cxgn_string_utils_free(utils);
        return 1;
    }

    if (!cxgn_struct_parser_parse_file(parser, args->header_path, &err)) {
        fprintf(stderr, "Error parsing header: %s\n", err.message);
        cxgn_struct_parser_free(parser);
        cxgn_string_utils_free(utils);
        return 1;
    }

    if (args->verbose)
        printf("Found %zu struct(s)\n", cxgn_struct_parser_get_struct_count(parser));

    gen = cxgn_generator_new(parser, utils);
    if (!gen) {
        fprintf(stderr, "Error: out of memory\n");
        cxgn_struct_parser_free(parser);
        cxgn_string_utils_free(utils);
        return 1;
    }

    cxgn_validation_options_init(&validation);
    validation.strict_mode = args->strict;
    validation.diagnostic_fn = cxgn_cli_diagnostic_sink;
    cxgn_generator_set_validation_options(gen, &validation);
    if (args->root_struct && args->root_struct[0] != '\0') {
        cxgn_generator_set_root_struct(gen, args->root_struct);
    }

    result = cxgn_cli_needs_batch(args)
        ? cxgn_cli_run_batch(args, gen, parser, utils)
        : cxgn_cli_run_single(args, gen, parser, utils);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    return result;
}

static char* cxgn_cli_resolve_plan_path(const char* plan_dir, const char* value) {
    if (!value) return NULL;
    if (value[0] == '/') return cxgn_cli_duplicate_string(value);
    return cxgn_cli_path_join_local(plan_dir ? plan_dir : ".", value);
}

static bool cxgn_cli_plan_job_collect_yaml(cli_args* job,
                                  const char* plan_dir,
                                  const cxgn_node* yaml_node) {
    size_t count;
    if (!job || !yaml_node) return false;

    if (cxgn_node_get_type(yaml_node) == CXGN_NODE_STRING) {
        char* resolved = cxgn_cli_resolve_plan_path(plan_dir, cxgn_cli_node_string_value(yaml_node));
        if (!resolved) return false;
        if (!cxgn_cli_args_set_yaml_copy(job, resolved)) {
            free(resolved);
            return false;
        }
        free(resolved);
        return true;
    }

    if (cxgn_node_get_type(yaml_node) != CXGN_NODE_ARRAY) return false;
    count = cxgn_node_array_count(yaml_node);
    for (size_t i = 0; i < count; i++) {
        const cxgn_node* item = cxgn_node_array_at(yaml_node, i);
        char* resolved;
        const char* item_value = cxgn_cli_node_string_value(item);
        if (!item_value) return false;
        resolved = cxgn_cli_resolve_plan_path(plan_dir, item_value);
        if (!resolved) return false;
        if (!cxgn_cli_args_set_yaml_copy(job, resolved)) {
            free(resolved);
            return false;
        }
        free(resolved);
    }
    return true;
}

static int cxgn_cli_run_plan(const cli_args* args) {
    cxgn_error err = {0};
    cxgn_document* doc = cxgn_document_from_yaml_file(args->plan_path, &err);
    const cxgn_node* root;
    const cxgn_node* codegen_node;
    const cxgn_node* jobs_node;
    char* plan_dir = NULL;
    size_t job_count;
    int result = 0;

    if (!doc) {
        fprintf(stderr, "Error reading plan: %s\n", err.message ? err.message : "unknown error");
        return 1;
    }

    root = cxgn_document_get_root(doc);
    if (!root || cxgn_node_get_type(root) != CXGN_NODE_OBJECT) {
        fprintf(stderr, "Error: plan YAML root must be a mapping\n");
        cxgn_document_free(doc);
        return 1;
    }

    codegen_node = cxgn_cli_find_object_key(root, "codegen");
    if (!codegen_node) {
        fprintf(stderr, "Error: plan YAML must contain a top-level codegen key\n");
        cxgn_document_free(doc);
        return 1;
    }

    jobs_node = codegen_node;
    if (cxgn_node_get_type(codegen_node) == CXGN_NODE_OBJECT) {
        const cxgn_node* nested = cxgn_cli_find_object_key(codegen_node, "jobs");
        if (!nested) nested = cxgn_cli_find_object_key(codegen_node, "list");
        if (nested) jobs_node = nested;
    }

    if (!jobs_node || cxgn_node_get_type(jobs_node) != CXGN_NODE_ARRAY) {
        fprintf(stderr, "Error: codegen must be a list or contain a jobs list\n");
        cxgn_document_free(doc);
        return 1;
    }

    plan_dir = cxgn_cli_path_get_directory(args->plan_path);
    if (!plan_dir) {
        fprintf(stderr, "Error: out of memory\n");
        cxgn_document_free(doc);
        return 1;
    }

    job_count = cxgn_node_array_count(jobs_node);
    for (size_t i = 0; i < job_count; i++) {
        const cxgn_node* job_node = cxgn_node_array_at(jobs_node, i);
        const cxgn_node* yaml_node;
        const char* header_value;
        const char* output_value;
        cli_args job = {0};
        char* header_path = NULL;
        char* output_path = NULL;
        char* map_root = NULL;

        if (!job_node || cxgn_node_get_type(job_node) != CXGN_NODE_OBJECT) {
            fprintf(stderr, "Error: codegen job %zu must be a mapping\n", i);
            result = 1;
            break;
        }

        yaml_node = cxgn_cli_find_object_key(job_node, "yaml");
        header_value = cxgn_cli_node_string_value(cxgn_cli_find_object_key(job_node, "header"));
        output_value = cxgn_cli_node_string_value(cxgn_cli_find_object_key(job_node, "output"));
        if (!yaml_node || !header_value || !output_value) {
            fprintf(stderr, "Error: codegen job %zu requires yaml, header and output\n", i);
            result = 1;
            break;
        }

        header_path = cxgn_cli_resolve_plan_path(plan_dir, header_value);
        output_path = cxgn_cli_resolve_plan_path(plan_dir, output_value);
        if (!header_path || !output_path) {
            fprintf(stderr, "Error: out of memory\n");
            free(header_path);
            free(output_path);
            result = 1;
            break;
        }

        job.header_path = header_path;
        job.output_path = output_path;
        job.header_include = cxgn_cli_node_string_value(cxgn_cli_find_object_key(job_node, "header_include"));
        job.helpers_header = cxgn_cli_node_string_value(cxgn_cli_find_object_key(job_node, "helpers_header"));
        job.root_struct = cxgn_cli_node_string_value(cxgn_cli_find_object_key(job_node, "root_struct"));
        job.map_name = cxgn_cli_node_string_value(cxgn_cli_find_object_key(job_node, "map_name"));
        job.map_type = cxgn_cli_node_string_value(cxgn_cli_find_object_key(job_node, "map_type"));
        job.strict = cxgn_cli_node_bool_value(cxgn_cli_find_object_key(job_node, "strict"), args->strict);
        job.verbose = args->verbose || cxgn_cli_node_bool_value(cxgn_cli_find_object_key(job_node, "verbose"), false);

        if (cxgn_cli_find_object_key(job_node, "map_root")) {
            map_root = cxgn_cli_resolve_plan_path(plan_dir, cxgn_cli_node_string_value(cxgn_cli_find_object_key(job_node, "map_root")));
            if (!map_root) {
                fprintf(stderr, "Error: out of memory\n");
                cxgn_cli_args_release_owned_yaml(&job);
                free(header_path);
                free(output_path);
                result = 1;
                break;
            }
            job.map_root = map_root;
        }

        if (!cxgn_cli_plan_job_collect_yaml(&job, plan_dir, yaml_node) || job.yaml_count == 0) {
            fprintf(stderr, "Error: codegen job %zu has invalid yaml value\n", i);
            cxgn_cli_args_release_owned_yaml(&job);
            free(map_root);
            free(header_path);
            free(output_path);
            result = 1;
            break;
        }

        if (job.verbose)
            printf("Running plan job %zu/%zu -> %s\n", i + 1, job_count, job.output_path);

        result = cxgn_cli_execute_job(&job);

        cxgn_cli_args_release_owned_yaml(&job);
        free(map_root);
        free(header_path);
        free(output_path);

        if (result != 0) break;
    }

    free(plan_dir);
    cxgn_document_free(doc);
    return result;
}

static void cxgn_cli_diagnostic_sink(cxgn_diagnostic_level level,
                                const cxgn_error* diagnostic,
                                void* userdata) {
    (void)userdata;
    if (!diagnostic) return;

    fprintf(stderr, "%s", level == CXGN_DIAGNOSTIC_ERROR ? "error" : "warning");
    if (diagnostic->path) {
        fprintf(stderr, ": %s", diagnostic->path);
        if (diagnostic->line > 0) {
            fprintf(stderr, ":%zu", diagnostic->line);
            if (diagnostic->column > 0) fprintf(stderr, ":%zu", diagnostic->column);
        }
    }
    fprintf(stderr, ": %s\n", diagnostic->message ? diagnostic->message : "validation diagnostic");
}

static bool cxgn_cli_emit_batch_sources_comment(FILE* outfile, const cxgn_batch* batch) {
    if (!outfile || !batch) return false;

    size_t count = cxgn_batch_count(batch);
    if (count == 0) return true;

    char** paths = (char**)calloc(count, sizeof(char*));
    if (!paths) return false;

    for (size_t i = 0; i < count; i++) {
        paths[i] = (char*)cxgn_batch_get_path(batch, i);
    }
    qsort(paths, count, sizeof(char*), cxgn_cli_cmp_path_str);

    fprintf(outfile, "// Sources: ");
    for (size_t i = 0; i < count; i++) {
        fprintf(outfile, "%s%s", i ? ", " : "", paths[i]);
    }
    fprintf(outfile, "\n");

    free(paths);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Single-file mode
 * ═══════════════════════════════════════════════════════════════════════════ */

static int cxgn_cli_run_single(const cli_args* args,
                      cxgn_generator* gen,
                      cxgn_struct_parser* parser,
                      cxgn_string_utils* utils) {
    (void)parser; (void)utils;
    cxgn_error err = {0};
    char* include_path = NULL;

    if (args->verbose)
        printf("Generating code from: %s\n", args->yaml_patterns[0]);

    cxgn_output* output = cxgn_generate(gen, args->yaml_patterns[0],
                                         args->header_path, &err);
    if (!output) {
        fprintf(stderr, "Error generating code: %s", err.message);
        if (err.path) fprintf(stderr, " at %s", err.path);
        fprintf(stderr, "\n");
        return 1;
    }

    if (args->header_include) include_path = cxgn_cli_duplicate_string(args->header_include);
    else include_path = cxgn_path_relative_to_file(args->output_path, args->header_path);
    if (!include_path) {
        fprintf(stderr, "Error: out of memory\n");
        cxgn_output_free(output);
        return 1;
    }

    FILE* outfile = fopen(args->output_path, "w");
    if (!outfile) {
        fprintf(stderr, "Error: cannot open output file %s\n", args->output_path);
        free(include_path);
        cxgn_output_free(output);
        return 1;
    }

    char* guard = cxgn_cli_make_include_guard(args->output_path);
    if (!guard) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(outfile);
        free(include_path);
        cxgn_output_free(output);
        return 1;
    }

    fprintf(outfile, "// GENERATED FILE - DO NOT EDIT\n");
    fprintf(outfile, "// Source YAML: %s\n", args->yaml_patterns[0]);
    fprintf(outfile, "// Source Header: %s\n", args->header_path);
    fprintf(outfile, "#ifndef %s\n#define %s\n\n", guard, guard);
    fprintf(outfile, "#include <stddef.h>\n");
    fprintf(outfile, "#include <stdbool.h>\n");
    if (args->helpers_header) {
        fprintf(outfile, "#include <%s>\n", args->helpers_header);
    }
    fprintf(outfile, "#include \"%s\"\n\n", include_path);
    fprintf(outfile, "%s", cxgn_output_get_code(output));
    fprintf(outfile, "\n#endif /* %s */\n", guard);

    fclose(outfile);
    free(guard);
    free(include_path);
    cxgn_output_free(output);

    if (args->verbose)
        printf("Generated: %s\n", args->output_path);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Batch mode
 * ═══════════════════════════════════════════════════════════════════════════ */

static int cxgn_cli_run_batch(const cli_args* args,
                     cxgn_generator* gen,
                     cxgn_struct_parser* parser,
                     cxgn_string_utils* utils) {
    (void)parser; (void)utils;
    cxgn_error err = {0};
    char* include_path = NULL;

    cxgn_batch* batch = cxgn_batch_new(gen);
    if (!batch) {
        fprintf(stderr, "Error: out of memory\n");
        return 1;
    }

    for (size_t i = 0; i < args->yaml_count; i++) {
        const char* pat = args->yaml_patterns[i];
        if (cxgn_cli_is_glob_pattern(pat)) {
            if (!cxgn_batch_add_glob(batch, pat, &err)) {
                fprintf(stderr, "Error expanding glob '%s': %s\n", pat, err.message);
                cxgn_batch_free(batch);
                return 1;
            }
        } else {
            if (!cxgn_batch_add_file(batch, pat, &err)) {
                fprintf(stderr, "Error: %s: %s\n", pat, err.message);
                cxgn_batch_free(batch);
                return 1;
            }
        }
    }

    if (cxgn_batch_count(batch) == 0) {
        fprintf(stderr, "Error: no YAML files matched\n");
        cxgn_batch_free(batch);
        return 1;
    }

    if (args->verbose)
        printf("Batch: %zu file(s)\n", cxgn_batch_count(batch));

    cxgn_batch_options bopts;
    cxgn_batch_options_init(&bopts);
    bopts.map_root = args->map_root;
    bopts.map_name = args->map_name;
    bopts.map_type = args->map_type;

    cxgn_output* output = cxgn_batch_generate(batch, args->header_path, &bopts, &err);

    if (!output) {
        fprintf(stderr, "Error generating batch: %s\n", err.message);
        cxgn_batch_free(batch);
        return 1;
    }

    if (args->header_include) include_path = cxgn_cli_duplicate_string(args->header_include);
    else include_path = cxgn_path_relative_to_file(args->output_path, args->header_path);
    if (!include_path) {
        fprintf(stderr, "Error: out of memory\n");
        cxgn_output_free(output);
        return 1;
    }

    FILE* outfile = fopen(args->output_path, "w");
    if (!outfile) {
        fprintf(stderr, "Error: cannot open output file %s\n", args->output_path);
        free(include_path);
        cxgn_output_free(output);
        return 1;
    }

    char* guard = cxgn_cli_make_include_guard(args->output_path);
    if (!guard) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(outfile);
        free(include_path);
        cxgn_output_free(output);
        return 1;
    }

    fprintf(outfile, "// GENERATED FILE - DO NOT EDIT\n");
    if (!cxgn_cli_emit_batch_sources_comment(outfile, batch)) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(outfile);
        free(guard);
        free(include_path);
        cxgn_output_free(output);
        cxgn_batch_free(batch);
        return 1;
    }
    fprintf(outfile, "#ifndef %s\n#define %s\n\n", guard, guard);
    fprintf(outfile, "#include <stddef.h>\n");
    fprintf(outfile, "#include <stdbool.h>\n");
    if (args->helpers_header) {
        fprintf(outfile, "#include <%s>\n", args->helpers_header);
    }
    fprintf(outfile, "#include \"%s\"\n\n", include_path);
    fprintf(outfile, "%s", cxgn_output_get_code(output));
    fprintf(outfile, "\n#endif /* %s */\n", guard);

    fclose(outfile);
    free(guard);
    free(include_path);
    cxgn_output_free(output);
    cxgn_batch_free(batch);

    if (args->verbose)
        printf("Generated: %s\n", args->output_path);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Compatibility shim for cxgn_parse_args (single-file embedding API)
 * ═══════════════════════════════════════════════════════════════════════════ */

bool cxgn_parse_args(int argc, char* argv[], cxgn_cli_args* args, cxgn_error* err) {
    if (!args) return false;
    memset(args, 0, sizeof(*args));
    args->cpp_std = CXGN_CPP_STD_20;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--yaml") == 0 || strcmp(argv[i], "-y") == 0) {
            if (i + 1 >= argc) {
                if (err) { err->code = CXGN_ERR_PARSE_ERROR; err->message = "Missing value for --yaml"; }
                return false;
            }
            args->yaml_path = argv[++i];
        } else if (strcmp(argv[i], "--header") == 0 || strcmp(argv[i], "-h") == 0) {
            if (i + 1 >= argc) {
                if (err) { err->code = CXGN_ERR_PARSE_ERROR; err->message = "Missing value for --header"; }
                return false;
            }
            args->header_path = argv[++i];
        } else if (strcmp(argv[i], "--plan") == 0) {
            if (i + 1 >= argc) {
                if (err) { err->code = CXGN_ERR_PARSE_ERROR; err->message = "Missing value for --plan"; }
                return false;
            }
            i++;
        } else if (strcmp(argv[i], "--header-include") == 0) {
            if (i + 1 >= argc) {
                if (err) { err->code = CXGN_ERR_PARSE_ERROR; err->message = "Missing value for --header-include"; }
                return false;
            }
            args->header_include = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                if (err) { err->code = CXGN_ERR_PARSE_ERROR; err->message = "Missing value for --output"; }
                return false;
            }
            args->output_path = argv[++i];
        } else if (strcmp(argv[i], "--helpers-header") == 0) {
            if (i + 1 >= argc) {
                if (err) { err->code = CXGN_ERR_PARSE_ERROR; err->message = "Missing value for --helpers-header"; }
                return false;
            }
            args->helpers_header = argv[++i];
        } else if (strcmp(argv[i], "--root-struct") == 0) {
            if (i + 1 >= argc) {
                if (err) { err->code = CXGN_ERR_PARSE_ERROR; err->message = "Missing value for --root-struct"; }
                return false;
            }
            args->root_struct = argv[++i];
        } else if (strcmp(argv[i], "--strict") == 0) {
            args->strict = true;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            args->verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            cxgn_cli_print_usage(argv[0]);
            exit(0);
        }
        /* Batch flags silently ignored in single-file compat mode */
    }

    if (!args->yaml_path || !args->header_path || !args->output_path) {
        if (err) { err->code = CXGN_ERR_PARSE_ERROR; err->message = "Missing required arguments"; }
        return false;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char* argv[]) {
    cli_args args;
    if (!cxgn_cli_parse_args(argc, argv, &args)) {
        cxgn_cli_print_usage(argv[0]);
        return 1;
    }

    int result = args.plan_path ? cxgn_cli_run_plan(&args) : cxgn_cli_execute_job(&args);
    cxgn_cli_args_free(&args);
    return result;
}
