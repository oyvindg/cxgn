/**
 * @file type_options.test.c
 * @brief Tests generator type output options.
 */

#include <cxgn/cxgn.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_custom_type_output_options(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    const bool parsed = cxgn_struct_parser_parse_file(parser, "fixtures/type_options.hpp", &err);
    assert(parsed);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    assert(gen != NULL);

    const cxgn_type_options opts = {
        .array_wrapper = "Vec",
        .optional_wrapper = "Maybe",
        .variant_wrapper = "std::variant",
        .array_ctor_fmt = "Vec<%s>{%s_data, %s}",
        .optional_empty_fmt = "Maybe<%s>::empty()",
        .optional_value_prefix_fmt = "Maybe<%s>{",
        .optional_value_suffix = "}"
    };
    cxgn_generator_set_type_options(gen, &opts);

    const char* yaml_text =
        "values: [1, 2, 3]\n"
        "max_items: null\n";

    cxgn_output* out = cxgn_generate_from_yaml_text(
        gen, yaml_text, "fixtures/type_options.yaml", "fixtures/type_options.hpp", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(code != NULL);
    assert(strstr(code, "Vec<int>{") != NULL);
    assert(strstr(code, "Maybe<int>::empty()") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_custom_type_output_options\n");
}

int main(void) {
    printf("Running type options tests...\n");
    test_custom_type_output_options();
    printf("All type options tests passed!\n");
    return 0;
}
