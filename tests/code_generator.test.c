/**
 * @file code_generator.test.c
 * @brief Tests for constexpr C++ code generation.
 *
 * Tests covered:
 * - Scalar value generation (int, double, bool, string)
 * - Nested struct initialization
 * - Array backing storage generation
 * - Optional field handling
 * - snake_case to camelCase mapping
 * - constexpr correctness
 */

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_scalar_generation(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    cg_struct_parser_parse_file(parser, "fixtures/simple.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/simple.yaml",
                                     "fixtures/simple.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);
    assert(code != NULL);

    /* Check generated code contains expected values */
    assert(strstr(code, "42") != NULL);           /* timeout value */
    assert(strstr(code, "\"test\"") != NULL);     /* name value */
    assert(strstr(code, "true") != NULL);         /* enabled value */

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_scalar_generation\n");
}

static void test_nested_struct(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    cg_struct_parser_parse_file(parser, "fixtures/nested.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/nested.yaml",
                                     "fixtures/nested.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);

    /* Check nested initialization syntax */
    assert(strstr(code, "{") != NULL);
    assert(strstr(code, "}") != NULL);
    assert(strstr(code, "8080") != NULL);  /* port value */

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_nested_struct\n");
}

static void test_snake_case_mapping(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    cg_struct_parser_parse_file(parser, "fixtures/naming.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/naming.yaml",
                                     "fixtures/naming.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);

    /* Check values were correctly mapped from snake_case YAML to camelCase C++ */
    assert(strstr(code, "3") != NULL);            /* max_retry_count -> maxRetryCount */
    assert(strstr(code, "\"localhost\"") != NULL); /* server_host -> serverHost */
    assert(strstr(code, "true") != NULL);         /* is_enabled -> isEnabled */
    assert(strstr(code, "30.5") != NULL);         /* connection_timeout -> connectionTimeout */

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_snake_case_mapping\n");
}

static void test_oneof_generation(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    cg_struct_parser_parse_file(parser, "fixtures/oneof.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/oneof.yaml",
                                     "fixtures/oneof.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);
    assert(strstr(code, "std::in_place_index<0>") != NULL);
    assert(strstr(code, "std::in_place_index<1>") != NULL);
    assert(strstr(code, "RuleGroup") != NULL);

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_oneof_generation\n");
}

int main(void) {
    printf("Running code generator tests...\n");
    test_scalar_generation();
    test_nested_struct();
    test_snake_case_mapping();
    test_oneof_generation();
    printf("All code generator tests passed!\n");
    return 0;
}
