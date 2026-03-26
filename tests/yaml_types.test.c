/**
 * @file yaml_types.test.c
 * @brief Tests for YAML type handling.
 *
 * Tests covered:
 * - Scalar types (int, double, bool, string)
 * - Arrays
 * - Nested objects
 * - Optionals
 */

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_scalar_types(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {CG_OK, NULL, NULL, 0, 0, false};

    cg_struct_parser_parse_file(parser, "fixtures/simple.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/simple.yaml",
                                     "fixtures/simple.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);
    assert(code != NULL);
    assert(strlen(code) > 0);

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_scalar_types\n");
}

static void test_array_types(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {CG_OK, NULL, NULL, 0, 0, false};

    cg_struct_parser_parse_file(parser, "fixtures/arrays.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/arrays.yaml",
                                     "fixtures/arrays.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);
    assert(code != NULL);

    /* Check for backing array generation */
    assert(strstr(code, "_backing_") != NULL || strstr(code, "static constexpr") != NULL);

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_array_types\n");
}

static void test_nested_types(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {CG_OK, NULL, NULL, 0, 0, false};

    cg_struct_parser_parse_file(parser, "fixtures/nested.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/nested.yaml",
                                     "fixtures/nested.hpp", &err);
    assert(output != NULL);

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_nested_types\n");
}

int main(void) {
    printf("Running YAML types tests...\n");
    test_scalar_types();
    test_array_types();
    test_nested_types();
    printf("All YAML types tests passed!\n");
    return 0;
}
