#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_simple_struct(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.h", &err));
    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, "SimpleConfig");
    assert(info != NULL);
    assert(cxgn_struct_get_field_count(info) == 3);
    assert(strcmp(cxgn_field_get_type(cxgn_struct_find_field(info, "name")), "const char*") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_helper_aliases(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/arrays.h", &err));
    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, "ArrayConfig");
    const cxgn_field_info* values = cxgn_struct_find_field(info, "values");
    const cxgn_field_info* items = cxgn_struct_find_field(info, "items");
    assert(values && items);
    assert(cxgn_field_is_array(values));
    assert(strcmp(cxgn_field_get_array_element_type(values), "int") == 0);
    assert(cxgn_field_is_array(items));
    assert(strcmp(cxgn_field_get_array_element_type(items), "ItemConfig") == 0);

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/optionals.h", &err));
    info = cxgn_struct_parser_find_struct(parser, "OptionalConfig");
    const cxgn_field_info* desc = cxgn_struct_find_field(info, "description");
    assert(desc && cxgn_field_is_optional(desc));
    assert(strcmp(cxgn_field_get_optional_value_type(desc), "const char*") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_include_following(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/main.h", &err));
    assert(cxgn_struct_parser_find_struct(parser, "MainConfig") != NULL);
    assert(cxgn_struct_parser_find_struct(parser, "SubConfig") != NULL);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

int main(void) {
    printf("Running struct parser tests...\n");
    test_simple_struct();
    test_helper_aliases();
    test_include_following();
    printf("All struct parser tests passed!\n");
    return 0;
}
