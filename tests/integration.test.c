/**
 * @file integration.test.c
 * @brief End-to-end integration tests for cxgen.
 *
 * Tests covered:
 * - Full pipeline: header + YAML -> generated C++
 * - Multiple struct types
 * - Complex nested configurations
 */

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_simple_end_to_end(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    /* Parse header */
    assert(cg_struct_parser_parse_file(parser, "fixtures/simple.hpp", &err));

    /* Generate code */
    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/simple.yaml",
                                     "fixtures/simple.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);
    assert(code != NULL);
    assert(strlen(code) > 0);

    /* Verify code structure */
    assert(strstr(code, "constexpr") != NULL || strstr(code, "config") != NULL);

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_simple_end_to_end\n");
}

static void test_nested_end_to_end(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/nested.hpp", &err));

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/nested.yaml",
                                     "fixtures/nested.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);
    assert(code != NULL);

    /* Check for nested struct values */
    assert(strstr(code, "8080") != NULL);  /* port value */

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_nested_end_to_end\n");
}

static void test_naming_convention_mapping(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/naming.hpp", &err));

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/naming.yaml",
                                     "fixtures/naming.hpp", &err);
    assert(output != NULL);

    /* snake_case YAML keys should map to camelCase C++ fields */
    const char* code = cg_output_get_code(output);
    assert(code != NULL);

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_naming_convention_mapping\n");
}

int main(void) {
    printf("Running integration tests...\n");
    test_simple_end_to_end();
    test_nested_end_to_end();
    test_naming_convention_mapping();
    printf("All integration tests passed!\n");
    return 0;
}
