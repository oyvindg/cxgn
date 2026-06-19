#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_helpers_header_override(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/scene.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_generator_set_helpers_header(gen, "cxgn/macros.h");
    cxgn_output* out = cxgn_generate(gen, "fixtures/scene.yaml", "fixtures/scene.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "#include <cxgn/macros.h>") != NULL);
    assert(strstr(code, "typedef struct { const Point2d* data; size_t count; }") == NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

int main(void) {
    printf("Running integration tests...\n");
    test_helpers_header_override();
    printf("All integration tests passed!\n");
    return 0;
}
