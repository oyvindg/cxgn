/**
 * @file hexapod_leg.test.c
 * @brief Tests for HexapodLeg struct parsing and code generation.
 *
 * Tests covered:
 * - Relative #include following (HexapodLeg.hpp -> Vec3.hpp)
 * - Multi-declaration parsing of Vec3 fields (float x, y, z;)
 * - Two-level nesting: Vec3 and Joint inside HexapodLeg
 * - snake_case YAML keys mapped to camelCase C++ fields
 * - Generated code contains expected values
 */

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_struct_parsing(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/HexapodLeg.hpp", &err));

    /* Both structs must be found — Vec3 via followed include */
    assert(cg_struct_parser_find_struct(parser, "Vec3")       != NULL);
    assert(cg_struct_parser_find_struct(parser, "Joint")      != NULL);
    assert(cg_struct_parser_find_struct(parser, "HexapodLeg") != NULL);

    /* Vec3: float x, y, z; expands to three fields */
    const cg_struct_info* vec3 = cg_struct_parser_find_struct(parser, "Vec3");
    assert(cg_struct_get_field_count(vec3) == 3);
    assert(cg_struct_find_field(vec3, "x") != NULL);
    assert(cg_struct_find_field(vec3, "y") != NULL);
    assert(cg_struct_find_field(vec3, "z") != NULL);

    /* Joint: length, min, max */
    const cg_struct_info* joint = cg_struct_parser_find_struct(parser, "Joint");
    assert(cg_struct_get_field_count(joint) == 3);
    assert(cg_struct_find_field(joint, "length") != NULL);
    assert(cg_struct_find_field(joint, "min")    != NULL);
    assert(cg_struct_find_field(joint, "max")    != NULL);

    /* HexapodLeg: index, basePosition, restPosition, coxa, femur, tibia */
    const cg_struct_info* leg = cg_struct_parser_find_struct(parser, "HexapodLeg");
    assert(cg_struct_get_field_count(leg) == 6);
    assert(cg_struct_find_field(leg, "index")        != NULL);
    assert(cg_struct_find_field(leg, "basePosition") != NULL);
    assert(cg_struct_find_field(leg, "restPosition") != NULL);
    assert(cg_struct_find_field(leg, "coxa")         != NULL);
    assert(cg_struct_find_field(leg, "femur")        != NULL);
    assert(cg_struct_find_field(leg, "tibia")        != NULL);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_struct_parsing\n");
}

static void test_field_types(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/HexapodLeg.hpp", &err));

    const cg_struct_info* vec3  = cg_struct_parser_find_struct(parser, "Vec3");
    const cg_struct_info* joint = cg_struct_parser_find_struct(parser, "Joint");
    const cg_struct_info* leg   = cg_struct_parser_find_struct(parser, "HexapodLeg");

    assert(strcmp(cg_field_get_type(cg_struct_find_field(vec3,  "x")),            "float") == 0);
    assert(strcmp(cg_field_get_type(cg_struct_find_field(joint, "length")),        "float") == 0);
    assert(strcmp(cg_field_get_type(cg_struct_find_field(leg,   "index")),         "int")   == 0);
    assert(strcmp(cg_field_get_type(cg_struct_find_field(leg,   "basePosition")), "Vec3")  == 0);
    assert(strcmp(cg_field_get_type(cg_struct_find_field(leg,   "coxa")),         "Joint") == 0);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_field_types\n");
}

static void test_generation(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    cg_struct_parser_parse_file(parser, "fixtures/HexapodLeg.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = cg_generate(gen, "fixtures/HexapodLeg.yaml",
                                     "fixtures/HexapodLeg.hpp", &err);
    assert(output != NULL);

    const char* code = cg_output_get_code(output);
    assert(code != NULL);

    /* Positions */
    assert(strstr(code, "80")   != NULL);   /* base_position.x */
    assert(strstr(code, "130")  != NULL);   /* rest_position.x */
    assert(strstr(code, "-40")  != NULL);   /* rest_position.y */
    assert(strstr(code, "-60")  != NULL);   /* rest_position.z */

    /* Segment lengths */
    assert(strstr(code, "52")   != NULL);   /* coxa.length  */
    assert(strstr(code, "66")   != NULL);   /* femur.length */

    /* Joint limits */
    assert(strstr(code, "-45")  != NULL);   /* coxa.min  */
    assert(strstr(code, "-150") != NULL);   /* tibia.min */

    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_generation\n");
}

int main(void) {
    printf("Running HexapodLeg tests...\n");
    test_struct_parsing();
    test_field_types();
    test_generation();
    printf("All HexapodLeg tests passed!\n");
    return 0;
}
