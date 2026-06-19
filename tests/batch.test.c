/**
 * @file batch.test.c
 * @brief Unit tests for cxgn batch generation.
 *
 * Tests covered:
 * - Two explicit files via cxgn_batch_add_file: combined output, prefixed names
 * - Map entry correctness (keys, variable references)
 * - Symbol prefix isolation (no name collisions in combined output)
 * - Glob expansion: single directory (*.yaml)
 * - Glob expansion: recursive (** / *.yaml)
 * - Key derivation without map_root (stem only)
 * - Key derivation with map_root (relative path)
 * - Collision detection (two files with the same stem, no map_root)
 * - Custom map_name / map_type options
 * - cxgn_batch_add_file error on missing file
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static cxgn_generator* make_gen(cxgn_struct_parser** out_parser,
                                 cxgn_string_utils** out_utils) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    assert(utils != NULL);
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(parser != NULL);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.h", &err));
    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    assert(gen != NULL);
    *out_parser = parser;
    *out_utils  = utils;
    return gen;
}

static void free_gen(cxgn_generator* gen, cxgn_struct_parser* parser,
                     cxgn_string_utils* utils) {
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_two_explicit_files(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);

    assert(cxgn_batch_add_file(batch, "fixtures/batch/alpha.yaml", &err));
    assert(cxgn_batch_add_file(batch, "fixtures/batch/beta.yaml",  &err));
    assert(cxgn_batch_count(batch) == 2);

    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", NULL, &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);

    /* Each entry must have a prefixed config variable */
    assert(strstr(code, "static const SimpleConfig alpha_config =") != NULL);
    assert(strstr(code, "static const SimpleConfig beta_config =")  != NULL);

    /* Original unprefixed name must NOT appear */
    assert(strstr(code, "static const SimpleConfig config =") == NULL);

    /* Map section */
    assert(strstr(code, "typedef struct {")             != NULL);
    assert(strstr(code, "const SimpleConfig* config;")  != NULL);
    assert(strstr(code, "cxgn_map_entry_t")             != NULL);
    assert(strstr(code, "typedef struct config_registry_t {") != NULL);
    assert(strstr(code, "static const cxgn_map_entry_t _config_entries[]") != NULL);
    assert(strstr(code, "static const config_registry_t config =") != NULL);
    assert(strstr(code, ".entries = _config_entries")   != NULL);
    assert(strstr(code, ".count = 2")                   != NULL);
    assert(strstr(code, "\"alpha\"")                    != NULL);
    assert(strstr(code, "&alpha_config")                != NULL);
    assert(strstr(code, "\"beta\"")                     != NULL);
    assert(strstr(code, "&beta_config")                 != NULL);

    cxgn_output_free(out);
    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_prefix_isolation(void) {
    /* Backing arrays must also carry the prefix so there are no collisions
       even when both configs contain array or struct fields.
       For SimpleConfig there are no arrays, but we verify the backing names
       do NOT appear without a prefix. */
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);
    assert(cxgn_batch_add_file(batch, "fixtures/batch/alpha.yaml", &err));
    assert(cxgn_batch_add_file(batch, "fixtures/batch/beta.yaml",  &err));

    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", NULL, &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    /* Ensure no bare "_backing_" without a prefix appears */
    const char* bare = strstr(code, "_backing_");
    if (bare) {
        /* If found, it must be preceded by an identifier character (the prefix) */
        assert(bare > code && (*(bare - 1) != '\n') && (*(bare - 1) != ' '));
    }

    cxgn_output_free(out);
    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_glob_single_dir(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);

    assert(cxgn_batch_add_glob(batch, "fixtures/batch/*.yaml", &err));
    /* alpha.yaml and beta.yaml but NOT sub/gamma.yaml */
    assert(cxgn_batch_count(batch) == 2);

    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", NULL, &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "alpha_config") != NULL);
    assert(strstr(code, "beta_config")  != NULL);
    assert(strstr(code, "gamma_config") == NULL);

    cxgn_output_free(out);
    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_glob_recursive(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);

    assert(cxgn_batch_add_glob(batch, "fixtures/batch/**/*.yaml", &err));
    /* alpha.yaml, beta.yaml, sub/alpha.yaml, and sub/gamma.yaml */
    assert(cxgn_batch_count(batch) == 4);

    cxgn_batch_options opts = {.map_root = "fixtures/batch"};
    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", &opts, &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "alpha_config") != NULL);
    assert(strstr(code, "beta_config")  != NULL);
    assert(strstr(code, "sub_alpha_config") != NULL);
    assert(strstr(code, "gamma_config") != NULL);

    cxgn_output_free(out);
    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_key_with_map_root(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);
    assert(cxgn_batch_add_file(batch, "fixtures/batch/sub/gamma.yaml", &err));

    cxgn_batch_options opts = {.map_root = "fixtures/batch"};
    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", &opts, &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    /* Key should be "sub/gamma", identifier "sub_gamma" */
    assert(strstr(code, "\"sub/gamma\"")    != NULL);
    assert(strstr(code, "sub_gamma_config") != NULL);

    cxgn_output_free(out);
    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_collision_detection(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);
    /* Two files with the same stem in different directories */
    assert(cxgn_batch_add_file(batch, "fixtures/batch/alpha.yaml",     &err));
    assert(cxgn_batch_add_file(batch, "fixtures/batch/sub/alpha.yaml", &err));

    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", NULL, &err);
    assert(out == NULL);
    assert(err.code == CXGN_ERR_PARSE_ERROR);
    assert(err.message != NULL);
    cxgn_batch_free(batch);

    free_gen(gen, parser, utils);
}

static void test_custom_map_names(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);
    assert(cxgn_batch_add_file(batch, "fixtures/batch/alpha.yaml", &err));

    cxgn_batch_options opts = {
        .map_name = "my_registry",
        .map_type = "my_entry_t",
    };
    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", &opts, &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "my_entry_t")   != NULL);
    assert(strstr(code, "static const my_entry_t _my_registry_entries[]") != NULL);
    assert(strstr(code, "typedef struct my_registry_registry_t {") != NULL);
    assert(strstr(code, "static const my_registry_registry_t my_registry =") != NULL);
    assert(strstr(code, ".entries = _my_registry_entries") != NULL);
    assert(strstr(code, ".count = 1") != NULL);

    cxgn_output_free(out);
    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_detailed_result_exposes_per_file_metadata(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch_result result = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);
    assert(cxgn_batch_add_file(batch, "fixtures/batch/beta.yaml",  &err));
    assert(cxgn_batch_add_file(batch, "fixtures/batch/alpha.yaml", &err));

    cxgn_batch_options opts;
    cxgn_batch_options_init(&opts);
    assert(cxgn_batch_generate_detailed(batch, "fixtures/simple.h", &opts, &result, &err));
    assert(result.combined_output != NULL);
    assert(result.entry_count == 2);
    assert(result.success_count == 2);
    assert(result.failure_count == 0);

    assert(strcmp(result.entries[0].yaml_path, "fixtures/batch/alpha.yaml") == 0);
    assert(strcmp(result.entries[0].key, "alpha") == 0);
    assert(strcmp(result.entries[0].identifier, "alpha") == 0);
    assert(result.entries[0].output != NULL);
    assert(result.entries[0].error.code == CXGN_OK);

    assert(strcmp(result.entries[1].yaml_path, "fixtures/batch/beta.yaml") == 0);
    assert(strcmp(result.entries[1].key, "beta") == 0);
    assert(strcmp(result.entries[1].identifier, "beta") == 0);
    assert(result.entries[1].output != NULL);
    assert(result.entries[1].error.code == CXGN_OK);

    cxgn_batch_result_clear(&result);
    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_continue_on_error_keeps_successful_entries(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_generator_set_strict_mode(gen, true);

    cxgn_error err = {0};
    cxgn_batch_result result = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);
    assert(cxgn_batch_add_file(batch, "fixtures/batch/alpha.yaml", &err));
    assert(cxgn_batch_add_file(batch, "fixtures/invalid_missing_simple.yaml", &err));

    cxgn_batch_options opts;
    cxgn_batch_options_init(&opts);
    opts.continue_on_error = true;

    assert(cxgn_batch_generate_detailed(batch, "fixtures/simple.h", &opts, &result, &err));
    assert(result.combined_output != NULL);
    assert(result.entry_count == 2);
    assert(result.success_count == 1);
    assert(result.failure_count == 1);

    const char* combined = cxgn_output_get_code(result.combined_output);
    assert(strstr(combined, "alpha_config") != NULL);
    assert(strstr(combined, "invalid_missing_config") == NULL);
    assert(strstr(combined, "static const config_registry_t config =") != NULL);
    assert(strstr(combined, ".count = 1") != NULL);

    assert(result.entries[0].output != NULL);
    assert(result.entries[1].output == NULL);
    assert(result.entries[1].error.code == CXGN_ERR_MISSING_FIELD);
    assert(result.entries[1].error.message != NULL);

    cxgn_batch_result_clear(&result);
    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_explicit_files_are_sorted(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);
    assert(cxgn_batch_add_file(batch, "fixtures/batch/beta.yaml",  &err));
    assert(cxgn_batch_add_file(batch, "fixtures/batch/alpha.yaml", &err));

    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", NULL, &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    const char* alpha = strstr(code, "\"alpha\"");
    const char* beta = strstr(code, "\"beta\"");
    assert(alpha != NULL);
    assert(beta != NULL);
    assert(alpha < beta);

    cxgn_output_free(out);
    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_map_root_requires_prefix_match(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);
    assert(cxgn_batch_add_file(batch, "fixtures/batch/alpha.yaml", &err));

    cxgn_batch_options opts = {.map_root = "fixtures/other"};
    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", &opts, &err);
    assert(out == NULL);
    assert(err.code == CXGN_ERR_PARSE_ERROR);
    assert(err.message != NULL);

    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_add_file_missing(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);

    bool ok = cxgn_batch_add_file(batch, "fixtures/nonexistent.yaml", &err);
    assert(!ok);
    assert(err.code == CXGN_ERR_FILE_NOT_FOUND);
    assert(cxgn_batch_count(batch) == 0);

    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

static void test_glob_zero_matches(void) {
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_gen(&parser, &utils);

    cxgn_error err = {0};
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);

    /* Zero matches is not an error */
    bool ok = cxgn_batch_add_glob(batch, "fixtures/batch/*.nonexistent", &err);
    assert(ok);
    assert(cxgn_batch_count(batch) == 0);

    cxgn_batch_free(batch);
    free_gen(gen, parser, utils);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("Running batch generation tests...\n");
    test_two_explicit_files();
    test_prefix_isolation();
    test_glob_single_dir();
    test_glob_recursive();
    test_key_with_map_root();
    test_collision_detection();
    test_custom_map_names();
    test_detailed_result_exposes_per_file_metadata();
    test_continue_on_error_keeps_successful_entries();
    test_explicit_files_are_sorted();
    test_map_root_requires_prefix_match();
    test_add_file_missing();
    test_glob_zero_matches();
    printf("All batch tests passed!\n");
    return 0;
}
