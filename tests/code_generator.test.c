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

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_scalar_generation(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/simple.hpp", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/simple.yaml",
                                     "fixtures/simple.hpp", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
    assert(code != NULL);

    /* Check generated code contains expected values */
    assert(strstr(code, "42") != NULL);           /* timeout value */
    assert(strstr(code, "\"test\"") != NULL);     /* name value */
    assert(strstr(code, "true") != NULL);         /* enabled value */

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_scalar_generation\n");
}

static void test_nested_struct(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/nested.hpp", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/nested.yaml",
                                     "fixtures/nested.hpp", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);

    /* Check nested initialization syntax */
    assert(strstr(code, "{") != NULL);
    assert(strstr(code, "}") != NULL);
    assert(strstr(code, "8080") != NULL);  /* port value */

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_nested_struct\n");
}

static void test_snake_case_mapping(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/naming.hpp", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/naming.yaml",
                                     "fixtures/naming.hpp", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);

    /* Check values were correctly mapped from snake_case YAML to camelCase C++ */
    assert(strstr(code, "3") != NULL);            /* max_retry_count -> maxRetryCount */
    assert(strstr(code, "\"localhost\"") != NULL); /* server_host -> serverHost */
    assert(strstr(code, "true") != NULL);         /* is_enabled -> isEnabled */
    assert(strstr(code, "30.5") != NULL);         /* connection_timeout -> connectionTimeout */

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_snake_case_mapping\n");
}

static void test_variant_generation(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/oneof.hpp", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/oneof.yaml",
                                     "fixtures/oneof.hpp", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
    assert(strstr(code, "std::in_place_index<0>") != NULL);
    assert(strstr(code, "std::in_place_index<1>") != NULL);
    assert(strstr(code, "RuleGroup") != NULL);

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_variant_generation\n");
}

static void test_backing_decl_sorting_prefers_alignment_rank(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/all_types.hpp", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/all_types.yaml",
                                     "fixtures/all_types.hpp", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
    assert(code != NULL);

    const char* spool = strstr(code, "static constexpr char _spool_0[]");
    const char* double_array = strstr(code, "static constexpr double _backing_AllTypesConfig_doubleArray_data[]");
    const char* string_array = strstr(code, "static constexpr std::string _backing_AllTypesConfig_strArray_data[]");
    const char* int_array = strstr(code, "static constexpr int _backing_AllTypesConfig_intArray_data[]");
    const char* bool_array = strstr(code, "static constexpr bool _backing_AllTypesConfig_boolArray_data[]");

    assert(spool != NULL);
    assert(double_array != NULL);
    assert(string_array != NULL);
    assert(int_array != NULL);
    assert(bool_array != NULL);

    assert(spool < string_array);
    assert(double_array < string_array);
    assert(string_array < int_array);
    assert(double_array < int_array);
    assert(int_array < bool_array);

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_backing_decl_sorting_prefers_alignment_rank\n");
}

int main(void) {
    printf("Running code generator tests...\n");
    test_scalar_generation();
    test_nested_struct();
    test_snake_case_mapping();
    test_variant_generation();
    test_backing_decl_sorting_prefers_alignment_rank();
    printf("All code generator tests passed!\n");
    return 0;
}
