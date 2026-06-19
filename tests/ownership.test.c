#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_generator_retains_parser_and_utils(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(utils != NULL);
    assert(parser != NULL);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/scene.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, NULL);
    assert(gen != NULL);

    cxgn_string_utils_free(utils);
    cxgn_struct_parser_free(parser);

    cxgn_output* out = cxgn_generate(gen, "fixtures/scene.yaml", "fixtures/scene.h", &err);
    assert(out != NULL);
    assert(strstr(cxgn_output_get_code(out), "static const SceneConfig config =") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    printf("  ✓ test_generator_retains_parser_and_utils\n");
}

static void test_output_retain_semantics(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/scene.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, NULL);
    cxgn_output* out = cxgn_generate(gen, "fixtures/scene.yaml", "fixtures/scene.h", &err);
    assert(out != NULL);

    cxgn_output* alias = cxgn_output_retain(out);
    cxgn_output_free(out);

    assert(strstr(cxgn_output_get_code(alias), ".skybox = 0") != NULL);

    cxgn_output_free(alias);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_output_retain_semantics\n");
}

static void test_batch_retains_generator_chain(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, NULL);
    cxgn_batch* batch = cxgn_batch_new(gen);
    assert(batch != NULL);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);

    assert(cxgn_batch_add_file(batch, "fixtures/batch/alpha.yaml", &err));

    cxgn_output* out = cxgn_batch_generate(batch, "fixtures/simple.h", NULL, &err);
    assert(out != NULL);
    assert(strstr(cxgn_output_get_code(out), "alpha_config") != NULL);

    cxgn_output_free(out);
    cxgn_batch_free(batch);
    printf("  ✓ test_batch_retains_generator_chain\n");
}

int main(void) {
    printf("Running ownership tests...\n");
    test_generator_retains_parser_and_utils();
    test_output_retain_semantics();
    test_batch_retains_generator_chain();
    printf("All ownership tests passed!\n");
    return 0;
}
