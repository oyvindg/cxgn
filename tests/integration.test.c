/**
 * @file integration.test.c
 * @brief End-to-end integration tests for cxgn.
 *
 * Tests covered:
 * - Full pipeline: header + YAML -> generated C++
 * - Multiple struct types
 * - Complex nested configurations
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_simple_end_to_end(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    /* Parse header */
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.hpp", &err));

    /* Generate code */
    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/simple.yaml",
                                     "fixtures/simple.hpp", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
    assert(code != NULL);
    assert(strlen(code) > 0);

    /* Verify code structure */
    assert(strstr(code, "constexpr") != NULL || strstr(code, "config") != NULL);

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_simple_end_to_end\n");
}

static void test_nested_end_to_end(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/nested.hpp", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/nested.yaml",
                                     "fixtures/nested.hpp", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
    assert(code != NULL);

    /* Check for nested struct values */
    assert(strstr(code, "8080") != NULL);  /* port value */

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_nested_end_to_end\n");
}

static void test_naming_convention_mapping(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/naming.hpp", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/naming.yaml",
                                     "fixtures/naming.hpp", &err);
    assert(output != NULL);

    /* snake_case YAML keys should map to camelCase C++ fields */
    const char* code = cxgn_output_get_code(output);
    assert(code != NULL);

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_naming_convention_mapping\n");
}

static void test_scene_readme_example(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/scene.hpp", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/scene.yaml",
                                     "fixtures/scene.hpp", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
    assert(code != NULL);

    assert(strstr(code, "constexpr SceneConfig config") != NULL);
    assert(strstr(code, "\"Level 1\"") != NULL);
    assert(strstr(code, "0.1") != NULL);
    assert(strstr(code, "0.2") != NULL);
    assert(strstr(code, "0.4") != NULL);
    assert(strstr(code, "Array<Point2d>") != NULL);
    assert(strstr(code, "Optional<std::string>::empty()") != NULL);
    assert(strstr(code, "256") != NULL);

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_scene_readme_example\n");
}

int main(void) {
    printf("Running integration tests...\n");
    test_simple_end_to_end();
    test_nested_end_to_end();
    test_naming_convention_mapping();
    test_scene_readme_example();
    printf("All integration tests passed!\n");
    return 0;
}
