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

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_multi_decl_parsing(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/Vec3.hpp", &err));

    const cg_struct_info* info = cg_struct_parser_find_struct(parser, "Vec3");
    assert(info != NULL);

    /* "float x, y, z;" must expand to three separate fields */
    assert(cg_struct_get_field_count(info) == 3);

    const cg_field_info* fx = cg_struct_find_field(info, "x");
    const cg_field_info* fy = cg_struct_find_field(info, "y");
    const cg_field_info* fz = cg_struct_find_field(info, "z");

    assert(fx != NULL);
    assert(fy != NULL);
    assert(fz != NULL);
    assert(strcmp(cg_field_get_type(fx), "float") == 0);
    assert(strcmp(cg_field_get_type(fy), "float") == 0);
    assert(strcmp(cg_field_get_type(fz), "float") == 0);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_multi_decl_parsing\n");
}

static void test_methods_are_ignored(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/Vec3.hpp", &err));

    const cg_struct_info* info = cg_struct_parser_find_struct(parser, "Vec3");
    assert(info != NULL);

    /* Methods, operators, and constructors must not appear as fields */
    assert(cg_struct_find_field(info, "dot")        == NULL);
    assert(cg_struct_find_field(info, "cross")      == NULL);
    assert(cg_struct_find_field(info, "length")     == NULL);
    assert(cg_struct_find_field(info, "normalized") == NULL);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_methods_are_ignored\n");
}

static void test_vec3_generation(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    cg_struct_parser_parse_file(parser, "fixtures/Vec3.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/Vec3.yaml",
                                     "fixtures/Vec3.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);
    assert(code != NULL);
    assert(strstr(code, "2.5")  != NULL);
    assert(strstr(code, "-0.5") != NULL);

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
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
