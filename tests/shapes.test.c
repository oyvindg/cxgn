#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_shapes_structs_generate_c_output(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/shapes.h", &err));
    assert(cxgn_struct_parser_find_struct(parser, "Circle") != NULL);
    assert(cxgn_struct_parser_find_struct(parser, "Rectangle") != NULL);
    assert(cxgn_struct_parser_find_struct(parser, "ShapeConfig") != NULL);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* output = cxgn_generate(gen, "fixtures/shapes_circle.yaml",
                                        "fixtures/shapes.h", &err);
    assert(output != NULL);

    const char* code = cxgn_output_get_code(output);
    assert(strstr(code, "static const ShapeConfig config =") != NULL);
    assert(strstr(code, ".shape = {") != NULL);
    assert(strstr(code, ".radius = 5.0") != NULL);
    assert(strstr(code, "std::") == NULL);

    cxgn_output_free(output);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

int main(void) {
    printf("Running shapes tests...\n");
    test_shapes_structs_generate_c_output();
    printf("All shapes tests passed!\n");
    return 0;
}
