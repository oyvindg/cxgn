#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_scalar_and_nested_generation(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/nested.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate(gen, "fixtures/nested.yaml", "fixtures/nested.h", &err);
    assert(out != NULL);
    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, ".name = \"app\"") != NULL);
    assert(strstr(code, ".port = 8080") != NULL);
    assert(strstr(code, ".enabled = true") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

int main(void) {
    printf("Running YAML type tests...\n");
    test_scalar_and_nested_generation();
    printf("All YAML type tests passed!\n");
    return 0;
}
