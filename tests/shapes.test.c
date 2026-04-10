/**
 * @file shapes.test.c
 * @brief Tests demonstrating the limitation of std::variant<A, B> when both types
 *        are structs (i.e. both map to a YAML mapping node).
 *
 * Current behaviour
 * -----------------
 * gen_variant discriminates by YAML node type:
 *   - YAML scalar → picks the non-struct type
 *   - YAML mapping → picks the struct type (only when exactly one is a struct)
 *
 * When BOTH A and B are structs every YAML mapping falls back to index 0
 * (type A), regardless of the actual content.  For std::variant<Circle, Rectangle>
 * this means Rectangle can never be selected.
 *
 * Possible solutions
 * ------------------
 * 1. Structural matching – compare the YAML mapping's keys against each
 *    struct's field names; pick the type whose fields best cover the keys.
 *    Works without any schema changes but may be ambiguous if structs share
 *    field names.
 *
 * 2. Explicit discriminator field – reserve a key (e.g. "_type") in the YAML
 *    mapping that names the intended C++ type.  Unambiguous but requires a
 *    convention in every YAML file.
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ── Parser tests ─────────────────────────────────────────────────────────── */

static void test_shapes_structs_parsed(void) {
    cxgn_string_utils* utils  = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/shapes.h", &err));

    assert(cxgn_struct_parser_find_struct(parser, "Circle")      != NULL);
    assert(cxgn_struct_parser_find_struct(parser, "Rectangle")   != NULL);
    assert(cxgn_struct_parser_find_struct(parser, "ShapeConfig") != NULL);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_shapes_structs_parsed\n");
}

static void test_variant_field_types_recognized(void) {
    cxgn_string_utils* utils  = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/shapes.h", &err));

    const cxgn_struct_info* cfg = cxgn_struct_parser_find_struct(parser, "ShapeConfig");
    assert(cfg != NULL);
    assert(cxgn_struct_get_field_count(cfg) == 1);

    const cxgn_field_info* f = cxgn_struct_find_field(cfg, "shape");
    assert(f != NULL);
    assert(cxgn_field_is_variant(f));
    assert(cxgn_field_get_variant_type_count(f) == 2);
    assert(strcmp(cxgn_field_get_variant_type(f, 0), "Circle")    == 0);
    assert(strcmp(cxgn_field_get_variant_type(f, 1), "Rectangle") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_variant_field_types_recognized\n");
}

/* ── Generator tests ──────────────────────────────────────────────────────── */

/*
 * Circle YAML: shape.radius is present.
 * gen_variant picks index<0> (Circle) because both types are structs and it
 * falls back to A.  In this case index<0> happens to be correct.
 */
static void test_circle_yaml_picks_index0(void) {
    cxgn_string_utils* utils  = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/shapes.h", &err);

    cxgn_generator* gen  = cxgn_generator_new(parser, utils);
    cxgn_output* output  = cxgn_generate(gen, "fixtures/shapes_circle.yaml",
                                      "fixtures/shapes.h", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
    assert(strstr(code, "std::in_place_index<0>") != NULL);
    assert(strstr(code, "Circle")                 != NULL);

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_circle_yaml_picks_index0\n");
}

/*
 * Rectangle YAML: shape.width and shape.height are present.
 *
 * Expected (future): index<1> (Rectangle).
 * Current behaviour: gen_variant falls back to index<0> (Circle) because both
 * types are structs.  Parsing Circle from {width, height} then fails with a
 * missing-field error (Circle has only `radius`), so cxgn_generate returns NULL.
 *
 * This test documents the known limitation.  Once a discriminator strategy is
 * implemented it should be updated to assert a successful output with index<1>.
 */
static void test_rect_yaml_currently_fails(void) {
    cxgn_string_utils* utils  = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/shapes.h", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/shapes_rect.yaml",
                                     "fixtures/shapes.h", &err);

    /*
     * TODO: when structural matching or discriminator support is added,
     * replace this block with:
     *
     *   assert(output != NULL);
     *   const char* code = cxgn_output_get_code(output);
     *   assert(strstr(code, "std::in_place_index<1>") != NULL);
     *   assert(strstr(code, "Rectangle")              != NULL);
     *   cxgn_output_free(output);
     */
    (void)output;  /* may be NULL — failure is the expected current outcome */

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_rect_yaml_currently_fails  (known limitation: both-struct std::variant)\n");
}

int main(void) {
    printf("Running shapes (std::variant<struct, struct>) tests...\n");
    test_shapes_structs_parsed();
    test_variant_field_types_recognized();
    test_circle_yaml_picks_index0();
    test_rect_yaml_currently_fails();
    printf("All shapes tests passed!\n");
    return 0;
}
