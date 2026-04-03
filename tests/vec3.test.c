/**
 * @file vec3.test.c
 * @brief Tests for multi-declaration field parsing (e.g. "float x, y, z;").
 *
 * Tests covered:
 * - Multi-declaration on one line expands to individual fields
 * - Field names and types are correct
 * - Methods and operators in the struct are ignored
 * - Code generation produces expected float values
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_multi_decl_parsing(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/Vec3.hpp", &err));

    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, "Vec3");
    assert(info != NULL);

    /* "float x, y, z;" must expand to three separate fields */
    assert(cxgn_struct_get_field_count(info) == 3);

    const cxgn_field_info* fx = cxgn_struct_find_field(info, "x");
    const cxgn_field_info* fy = cxgn_struct_find_field(info, "y");
    const cxgn_field_info* fz = cxgn_struct_find_field(info, "z");

    assert(fx != NULL);
    assert(fy != NULL);
    assert(fz != NULL);
    assert(strcmp(cxgn_field_get_type(fx), "float") == 0);
    assert(strcmp(cxgn_field_get_type(fy), "float") == 0);
    assert(strcmp(cxgn_field_get_type(fz), "float") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_multi_decl_parsing\n");
}

static void test_methods_are_ignored(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/Vec3.hpp", &err));

    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, "Vec3");
    assert(info != NULL);

    /* Methods, operators, and constructors must not appear as fields */
    assert(cxgn_struct_find_field(info, "dot")        == NULL);
    assert(cxgn_struct_find_field(info, "cross")      == NULL);
    assert(cxgn_struct_find_field(info, "length")     == NULL);
    assert(cxgn_struct_find_field(info, "normalized") == NULL);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_methods_are_ignored\n");
}

static void test_vec3_generation(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/Vec3.hpp", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/Vec3.yaml",
                                     "fixtures/Vec3.hpp", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
    assert(code != NULL);
    assert(strstr(code, "2.5")  != NULL);
    assert(strstr(code, "-0.5") != NULL);

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_vec3_generation\n");
}

int main(void) {
    printf("Running Vec3 tests...\n");
    test_multi_decl_parsing();
    test_methods_are_ignored();
    test_vec3_generation();
    printf("All Vec3 tests passed!\n");
    return 0;
}
