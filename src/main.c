/**
 * @file main.c
 * @brief Cxgn CLI tool.
 *
 * Single-file mode (existing):
 *   cxgn --yaml config.yaml --header Config.h --output config.gen.h
 *
 * Batch mode (multiple --yaml or glob pattern):
 *   cxgn --yaml "strategies/**.yaml"    --header Config.h --output all.gen.h
 *   cxgn --yaml a.yaml --yaml b.yaml   --header Config.h --output all.gen.h
 *
 * Batch options:
 *   --map-root <dir>    Strip this prefix from file paths when deriving map keys.
 *   --map-name <name>   C identifier for the emitted map array (default: cxgn_config_map).
 *   --map-type <name>   C identifier for the map entry typedef (default: cxgn_map_entry_t).
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

    const char* header_path;
    const char* output_path;
    const char* helpers_header;
    const char* map_root;
    const char* map_name;
    const char* map_type;
    bool        verbose;
} cli_args;

static void cli_args_free(cli_args* args) {
    free(args->yaml_patterns);
}

static bool cli_args_add_yaml(cli_args* args, const char* val) {
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

static bool is_option_token(const char* arg) {
    return arg && arg[0] == '-' && arg[1] != '\0';
}

static void print_usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s --yaml <file|pattern> --header <file> --output <file>\n"
            "          [--yaml <file|pattern>]...\n"
            "          [--map-root <dir>] [--map-name <name>] [--map-type <name>]\n"
            "          [--helpers-header <path>] [--verbose]\n"
            "\n"
            "Options:\n"
            "  --yaml, -y         YAML file or glob pattern (repeatable; can take multiple values)\n"
            "  --header, -h       C header with struct definitions\n"
            "  --output, -o       Output file path\n"
            "  --map-root         Directory prefix stripped from paths in batch keys\n"
            "  --map-name         C identifier for the map array (batch; default: cxgn_config_map)\n"
            "  --map-type         C identifier for the map entry typedef (batch; default: cxgn_map_entry_t)\n"
            "  --helpers-header   Emit #include <path> instead of inlining cxgn helper typedefs\n"
            "  --verbose, -v      Enable verbose output\n"
            "  --help             Show this help\n",
            prog);
}

static bool parse_args(int argc, char* argv[], cli_args* args) {
    memset(args, 0, sizeof(*args));

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];

#define REQUIRE_NEXT(flag) \
    do { if (i + 1 >= argc) { \
        fprintf(stderr, "Error: missing value for %s\n", flag); return false; \
    } } while (0)

        if (strcmp(a, "--yaml") == 0 || strcmp(a, "-y") == 0) {
            bool added = false;
            while (i + 1 < argc && !is_option_token(argv[i + 1])) {
                if (!cli_args_add_yaml(args, argv[++i])) {
                    fprintf(stderr, "Error: out of memory\n"); return false;
                }
                added = true;
            }
            if (!added) {
                fprintf(stderr, "Error: missing value for --yaml\n");
                return false;
            }
        } else if (strcmp(a, "--header") == 0 || strcmp(a, "-h") == 0) {
            REQUIRE_NEXT("--header");
            args->header_path = argv[++i];
        } else if (strcmp(a, "--output") == 0 || strcmp(a, "-o") == 0) {
            REQUIRE_NEXT("--output");
            args->output_path = argv[++i];
        } else if (strcmp(a, "--helpers-header") == 0) {
            REQUIRE_NEXT("--helpers-header");
            args->helpers_header = argv[++i];
        } else if (strcmp(a, "--map-root") == 0) {
            REQUIRE_NEXT("--map-root");
            args->map_root = argv[++i];
        } else if (strcmp(a, "--map-name") == 0) {
            REQUIRE_NEXT("--map-name");
            args->map_name = argv[++i];
        } else if (strcmp(a, "--map-type") == 0) {
            REQUIRE_NEXT("--map-type");
            args->map_type = argv[++i];
        } else if (strcmp(a, "--verbose") == 0 || strcmp(a, "-v") == 0) {
            args->verbose = true;
        } else if (strcmp(a, "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Error: unknown argument: %s\n", a);
            return false;
        }
#undef REQUIRE_NEXT
    }

    if (args->yaml_count == 0 || !args->header_path || !args->output_path) {
        fprintf(stderr, "Error: --yaml, --header and --output are required\n");
        return false;
    }
    return true;
}

/* Returns true when a string contains glob metacharacters or "**". */
static bool is_glob_pattern(const char* s) {
    if (strstr(s, "**")) return true;
    for (; *s; s++) {
        if (*s == '*' || *s == '?' || *s == '[') return true;
    }
    return false;
}

/* Returns true when batch mode should be used (multiple files or glob). */
static bool needs_batch(const cli_args* args) {
    if (args->yaml_count > 1) return true;
    return is_glob_pattern(args->yaml_patterns[0]);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Include guard from output path
 * ═══════════════════════════════════════════════════════════════════════════ */

static char* make_include_guard(const char* output_path) {
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
    guard[w++] = '_';
    guard[w++] = 'H';
    guard[w] = '\0';
    return guard;
}

static int cmp_path_str(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static bool emit_batch_sources_comment(FILE* outfile, const cxgn_batch* batch) {
    if (!outfile || !batch) return false;

    size_t count = cxgn_batch_count(batch);
    if (count == 0) return true;

    char** paths = (char**)calloc(count, sizeof(char*));
    if (!paths) return false;

    for (size_t i = 0; i < count; i++) {
        paths[i] = (char*)cxgn_batch_get_path(batch, i);
    }
    qsort(paths, count, sizeof(char*), cmp_path_str);

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

static int run_single(const cli_args* args,
                      cxgn_generator* gen,
                      cxgn_struct_parser* parser,
                      cxgn_string_utils* utils) {
    (void)parser; (void)utils;
    cxgn_error err = {0};

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

    char* include_path = cxgn_path_relative_to_file(args->output_path,
                                                      args->header_path);
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

    char* guard = make_include_guard(args->output_path);
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

static int run_batch(const cli_args* args,
                     cxgn_generator* gen,
                     cxgn_struct_parser* parser,
                     cxgn_string_utils* utils) {
    (void)parser; (void)utils;
    cxgn_error err = {0};

    cxgn_batch* batch = cxgn_batch_new(gen);
    if (!batch) {
        fprintf(stderr, "Error: out of memory\n");
        return 1;
    }

    for (size_t i = 0; i < args->yaml_count; i++) {
        const char* pat = args->yaml_patterns[i];
        if (is_glob_pattern(pat)) {
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

    cxgn_batch_options bopts = {
        .map_root = args->map_root,
        .map_name = args->map_name,
        .map_type = args->map_type,
    };

    cxgn_output* output = cxgn_batch_generate(batch, args->header_path, &bopts, &err);

    if (!output) {
        fprintf(stderr, "Error generating batch: %s\n", err.message);
        cxgn_batch_free(batch);
        return 1;
    }

    char* include_path = cxgn_path_relative_to_file(args->output_path,
                                                      args->header_path);
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

    char* guard = make_include_guard(args->output_path);
    if (!guard) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(outfile);
        free(include_path);
        cxgn_output_free(output);
        return 1;
    }

    fprintf(outfile, "// GENERATED FILE - DO NOT EDIT\n");
    if (!emit_batch_sources_comment(outfile, batch)) {
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
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            args->verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
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
    if (!parse_args(argc, argv, &args)) {
        print_usage(argv[0]);
        return 1;
    }

    if (args.verbose)
        printf("Parsing header: %s\n", args.header_path);

    cxgn_string_utils* utils = cxgn_string_utils_new();
    if (!utils) { fprintf(stderr, "Error: out of memory\n"); return 1; }

    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    if (!parser) {
        fprintf(stderr, "Error: out of memory\n");
        cxgn_string_utils_free(utils);
        return 1;
    }

    cxgn_error err = {0};
    if (!cxgn_struct_parser_parse_file(parser, args.header_path, &err)) {
        fprintf(stderr, "Error parsing header: %s\n", err.message);
        cxgn_struct_parser_free(parser);
        cxgn_string_utils_free(utils);
        cli_args_free(&args);
        return 1;
    }

    if (args.verbose)
        printf("Found %zu struct(s)\n", cxgn_struct_parser_get_struct_count(parser));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    if (!gen) {
        fprintf(stderr, "Error: out of memory\n");
        cxgn_struct_parser_free(parser);
        cxgn_string_utils_free(utils);
        cli_args_free(&args);
        return 1;
    }

    cxgn_generator_set_helpers_header(gen, args.helpers_header);

    int result = needs_batch(&args)
        ? run_batch(&args, gen, parser, utils)
        : run_single(&args, gen, parser, utils);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    cli_args_free(&args);
    return result;
}
