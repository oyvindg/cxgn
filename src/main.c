/**
 * @file main.c
 * @brief Cxgn CLI tool.
 *
 * Usage: cxgn --yaml config.yaml --header Config.hpp --output config.gen.hpp
 *
 * Reads struct definitions from header, YAML values from config,
 * generates constexpr C++ output.
 */

#include <cxgn/cxgn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Argument Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s --yaml <file> --header <file> --output <file> [--std <ver>] [--verbose]\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --yaml, -y     YAML configuration file\n");
    fprintf(stderr, "  --header, -h   C++ header file with struct definitions\n");
    fprintf(stderr, "  --output, -o   Output file for generated code\n");
    fprintf(stderr, "  --std          Target C++ standard: 17, 20 (default), or auto\n");
    fprintf(stderr, "                   auto  emits #if __cplusplus guards\n");
    fprintf(stderr, "  --verbose, -v  Enable verbose output\n");
    fprintf(stderr, "  --help         Show this help message\n");
}

bool cxgn_parse_args(int argc, char* argv[], cxgn_cli_args* args, cxgn_error* err) {
    if (!args) return false;

    memset(args, 0, sizeof(*args));
    args->cpp_std = CXGN_CPP_STD_20;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--yaml") == 0 || strcmp(argv[i], "-y") == 0) {
            if (i + 1 >= argc) {
                if (err) {
                    err->code = CXGN_ERR_PARSE_ERROR;
                    err->message = "Missing value for --yaml";
                }
                return false;
            }
            args->yaml_path = argv[++i];
        } else if (strcmp(argv[i], "--header") == 0 || strcmp(argv[i], "-h") == 0) {
            if (i + 1 >= argc) {
                if (err) {
                    err->code = CXGN_ERR_PARSE_ERROR;
                    err->message = "Missing value for --header";
                }
                return false;
            }
            args->header_path = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                if (err) {
                    err->code = CXGN_ERR_PARSE_ERROR;
                    err->message = "Missing value for --output";
                }
                return false;
            }
            args->output_path = argv[++i];
        } else if (strcmp(argv[i], "--std") == 0) {
            if (i + 1 >= argc) {
                if (err) {
                    err->code = CXGN_ERR_PARSE_ERROR;
                    err->message = "Missing value for --std";
                }
                return false;
            }
            const char* val = argv[++i];
            if (strcmp(val, "auto") == 0) {
                args->cpp_std = CXGN_CPP_STD_AUTO;
            } else if (strcmp(val, "17") == 0) {
                args->cpp_std = CXGN_CPP_STD_17;
            } else if (strcmp(val, "20") == 0) {
                args->cpp_std = CXGN_CPP_STD_20;
            } else {
                if (err) {
                    err->code = CXGN_ERR_PARSE_ERROR;
                    err->message = "Invalid --std value: expected 17, 20, or auto";
                }
                return false;
            }
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            args->verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            if (err) {
                err->code = CXGN_ERR_PARSE_ERROR;
                err->message = "Unknown argument";
            }
            return false;
        }
    }

    if (!args->yaml_path || !args->header_path || !args->output_path) {
        if (err) {
            err->code = CXGN_ERR_PARSE_ERROR;
            err->message = "Missing required arguments";
        }
        return false;
    }

    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char* argv[]) {
    cxgn_error err = {0};
    cxgn_cli_args args;

    if (!cxgn_parse_args(argc, argv, &args, &err)) {
        fprintf(stderr, "Error: %s\n", err.message);
        print_usage(argv[0]);
        return 1;
    }

    if (args.verbose) {
        printf("Parsing header: %s\n", args.header_path);
    }

    cxgn_string_utils* utils = cxgn_string_utils_new();
    if (!utils) {
        fprintf(stderr, "Error: Out of memory\n");
        return 1;
    }

    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    if (!parser) {
        fprintf(stderr, "Error: Out of memory\n");
        cxgn_string_utils_free(utils);
        return 1;
    }

    if (!cxgn_struct_parser_parse_file(parser, args.header_path, &err)) {
        fprintf(stderr, "Error parsing header: %s\n", err.message);
        cxgn_struct_parser_free(parser);
        cxgn_string_utils_free(utils);
        return 1;
    }

    if (args.verbose) {
        printf("Found %zu struct(s)\n", cxgn_struct_parser_get_struct_count(parser));
        printf("Generating code from: %s\n", args.yaml_path);
    }

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    if (!gen) {
        fprintf(stderr, "Error: Out of memory\n");
        cxgn_struct_parser_free(parser);
        cxgn_string_utils_free(utils);
        return 1;
    }

    cxgn_generator_set_cpp_std(gen, args.cpp_std);

    cxgn_output* output = cxgn_generate(gen, args.yaml_path, args.header_path, &err);

    if (!output) {
        fprintf(stderr, "Error generating code: %s", err.message);
        if (err.path) {
            fprintf(stderr, " at %s", err.path);
        }
        fprintf(stderr, "\n");
        cxgn_generator_free(gen);
        cxgn_struct_parser_free(parser);
        cxgn_string_utils_free(utils);
        return 1;
    }

    char* include_path = cxgn_path_relative_to_file(args.output_path, args.header_path);
    if (!include_path) {
        fprintf(stderr, "Error: Out of memory\n");
        cxgn_output_free(output);
        cxgn_generator_free(gen);
        cxgn_struct_parser_free(parser);
        cxgn_string_utils_free(utils);
        return 1;
    }

    FILE* outfile = fopen(args.output_path, "w");
    if (!outfile) {
        fprintf(stderr, "Error: Cannot open output file %s\n", args.output_path);
        free(include_path);
        cxgn_output_free(output);
        cxgn_generator_free(gen);
        cxgn_struct_parser_free(parser);
        cxgn_string_utils_free(utils);
        return 1;
    }

    fprintf(outfile, "// GENERATED FILE - DO NOT EDIT\n");
    fprintf(outfile, "// Source YAML: %s\n", args.yaml_path);
    fprintf(outfile, "// Source Header: %s\n", args.header_path);
    fprintf(outfile, "#pragma once\n\n");
    fprintf(outfile, "#include <variant>\n");
    fprintf(outfile, "#include \"%s\"\n\n", include_path);
    fprintf(outfile, "%s", cxgn_output_get_code(output));

    fclose(outfile);
    free(include_path);
    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);

    if (args.verbose) {
        printf("Generated: %s\n", args.output_path);
    }

    return 0;
}
