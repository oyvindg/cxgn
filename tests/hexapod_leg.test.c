/**
 * @file hexapod_leg.test.c
 * @brief Tests for HexapodLeg struct parsing and code generation.
 *
 * Tests covered:
 * - Relative #include following (HexapodLeg.h -> Vec3.h)
 * - Multi-declaration parsing of Vec3 fields (float x, y, z;)
 * - Two-level nesting: Vec3 and Joint inside HexapodLeg
 * - snake_case YAML keys mapped to camelCase C++ fields
 * - Generated code contains expected values
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_struct_parsing(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/HexapodLeg.h", &err));

    /* Both structs must be found — Vec3 via followed include */
    assert(cxgn_struct_parser_find_struct(parser, "Vec3")       != NULL);
    assert(cxgn_struct_parser_find_struct(parser, "Joint")      != NULL);
    assert(cxgn_struct_parser_find_struct(parser, "HexapodLeg") != NULL);

    /* Vec3: float x, y, z; expands to three fields */
    const cxgn_struct_info* vec3 = cxgn_struct_parser_find_struct(parser, "Vec3");
    assert(cxgn_struct_get_field_count(vec3) == 3);
    assert(cxgn_struct_find_field(vec3, "x") != NULL);
    assert(cxgn_struct_find_field(vec3, "y") != NULL);
    assert(cxgn_struct_find_field(vec3, "z") != NULL);

    /* Joint: length, min, max */
    const cxgn_struct_info* joint = cxgn_struct_parser_find_struct(parser, "Joint");
    assert(cxgn_struct_get_field_count(joint) == 3);
    assert(cxgn_struct_find_field(joint, "length") != NULL);
    assert(cxgn_struct_find_field(joint, "min")    != NULL);
    assert(cxgn_struct_find_field(joint, "max")    != NULL);

    /* HexapodLeg: index, basePosition, restPosition, coxa, femur, tibia */
    const cxgn_struct_info* leg = cxgn_struct_parser_find_struct(parser, "HexapodLeg");
    assert(cxgn_struct_get_field_count(leg) == 6);
    assert(cxgn_struct_find_field(leg, "index")        != NULL);
    assert(cxgn_struct_find_field(leg, "basePosition") != NULL);
    assert(cxgn_struct_find_field(leg, "restPosition") != NULL);
    assert(cxgn_struct_find_field(leg, "coxa")         != NULL);
    assert(cxgn_struct_find_field(leg, "femur")        != NULL);
    assert(cxgn_struct_find_field(leg, "tibia")        != NULL);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_struct_parsing\n");
}

static void test_field_types(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/HexapodLeg.h", &err));

    const cxgn_struct_info* vec3  = cxgn_struct_parser_find_struct(parser, "Vec3");
    const cxgn_struct_info* joint = cxgn_struct_parser_find_struct(parser, "Joint");
    const cxgn_struct_info* leg   = cxgn_struct_parser_find_struct(parser, "HexapodLeg");

    assert(strcmp(cxgn_field_get_type(cxgn_struct_find_field(vec3,  "x")),            "float") == 0);
    assert(strcmp(cxgn_field_get_type(cxgn_struct_find_field(joint, "length")),        "float") == 0);
    assert(strcmp(cxgn_field_get_type(cxgn_struct_find_field(leg,   "index")),         "int")   == 0);
    assert(strcmp(cxgn_field_get_type(cxgn_struct_find_field(leg,   "basePosition")), "Vec3")  == 0);
    assert(strcmp(cxgn_field_get_type(cxgn_struct_find_field(leg,   "coxa")),         "Joint") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_field_types\n");
}

static void test_generation(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/HexapodLeg.h", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/HexapodLeg.yaml",
                                     "fixtures/HexapodLeg.h", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
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

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
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
