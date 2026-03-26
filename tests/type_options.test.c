/**
 * @file type_options.test.c
 * @brief Tests generator type output options.
 */

#include <cxgen/cxgen.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_custom_type_output_options(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    const bool parsed = cg_struct_parser_parse_file(parser, "fixtures/type_options.hpp", &err);
    assert(parsed);

    cg_generator* gen = cg_generator_new(parser, utils);
    assert(gen != NULL);

    const cg_type_options opts = {
        .array_wrapper = "Vec",
        .optional_wrapper = "Maybe",
        .oneof_wrapper = "OneOf",
        .array_ctor_fmt = "Vec<%s>{%s_data, %s_count}",
        .optional_empty_fmt = "Maybe<%s>::empty()",
        .optional_value_prefix_fmt = "Maybe<%s>{",
        .optional_value_suffix = "}"
    };
    cg_generator_set_type_options(gen, &opts);

    const char* yaml_text =
        "values: [1, 2, 3]\n"
        "max_items: null\n";

    cg_output* out = cg_generate_from_yaml_text(
        gen, yaml_text, "fixtures/type_options.yaml", "fixtures/type_options.hpp", &err);
    assert(out != NULL);

    const char* code = cg_output_get_code(out);
    assert(code != NULL);
    assert(strstr(code, "Vec<int>{") != NULL);
    assert(strstr(code, "Maybe<int>::empty()") != NULL);

    cg_output_free(out);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_custom_type_output_options\n");
}

int main(void) {
    printf("Running type options tests...\n");
    test_custom_type_output_options();
    printf("All type options tests passed!\n");
    return 0;
}
