/**
 * @file struct_parser.test.c
 * @brief Tests for C++ struct definition parsing.
 *
 * Tests covered:
 * - Simple struct parsing
 * - Nested struct references
 * - Array<T> field detection
 * - Optional<T> field detection
 * - Default values
 * - #include following
 * - Multiple structs per file
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_simple_struct(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.hpp", &err));

    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, "SimpleConfig");
    assert(info != NULL);
    assert(strcmp(cxgn_struct_get_name(info), "SimpleConfig") == 0);

    /* Check fields */
    assert(cxgn_struct_get_field_count(info) == 3);

    const cxgn_field_info* field = cxgn_struct_get_field(info, 0);
    assert(strcmp(cxgn_field_get_name(field), "timeout") == 0);
    assert(strcmp(cxgn_field_get_type(field), "int") == 0);

    field = cxgn_struct_get_field(info, 1);
    assert(strcmp(cxgn_field_get_name(field), "name") == 0);
    assert(strcmp(cxgn_field_get_type(field), "std::string") == 0);

    field = cxgn_struct_get_field(info, 2);
    assert(strcmp(cxgn_field_get_name(field), "enabled") == 0);
    assert(strcmp(cxgn_field_get_type(field), "bool") == 0);
    assert(strcmp(cxgn_field_get_default(field), "true") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_simple_struct\n");
}

static void test_array_fields(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/arrays.hpp", &err));

    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, "ArrayConfig");
    assert(info != NULL);

    const cxgn_field_info* field = cxgn_struct_find_field(info, "values");
    assert(field != NULL);
    assert(cxgn_field_is_array(field));
    assert(strcmp(cxgn_field_get_array_element_type(field), "int") == 0);

    field = cxgn_struct_find_field(info, "items");
    assert(field != NULL);
    assert(cxgn_field_is_array(field));
    assert(strcmp(cxgn_field_get_array_element_type(field), "ItemConfig") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_array_fields\n");
}

static void test_optional_fields(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/optionals.hpp", &err));

    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, "OptionalConfig");
    assert(info != NULL);

    const cxgn_field_info* field = cxgn_struct_find_field(info, "description");
    assert(field != NULL);
    assert(cxgn_field_is_optional(field));
    assert(strcmp(cxgn_field_get_optional_value_type(field), "std::string") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_optional_fields\n");
}

static void test_variant_fields(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/oneof.hpp", &err));

    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, "VariantConfig");
    assert(info != NULL);

    const cxgn_field_info* field = cxgn_struct_find_field(info, "rules");
    assert(field != NULL);
    assert(cxgn_field_is_array(field));
    assert(strcmp(cxgn_field_get_array_element_type(field), "std::variant<std::string, RuleGroup>") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_variant_fields\n");
}

static void test_include_following(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    /* main.hpp includes types.hpp */
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/main.hpp", &err));

    /* Should find structs from both files */
    assert(cxgn_struct_parser_find_struct(parser, "MainConfig") != NULL);
    assert(cxgn_struct_parser_find_struct(parser, "SubConfig") != NULL);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_include_following\n");
}

static void test_builtin_types(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);

    assert(cxgn_struct_parser_is_builtin_type(parser, "int"));
    assert(cxgn_struct_parser_is_builtin_type(parser, "double"));
    assert(cxgn_struct_parser_is_builtin_type(parser, "bool"));
    assert(cxgn_struct_parser_is_builtin_type(parser, "std::string"));
    assert(cxgn_struct_parser_is_builtin_type(parser, "size_t"));

    assert(!cxgn_struct_parser_is_builtin_type(parser, "MyCustomType"));
    assert(!cxgn_struct_parser_is_builtin_type(parser, "ServerConfig"));

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_builtin_types\n");
}

int main(void) {
    printf("Running struct parser tests...\n");
    test_simple_struct();
    test_array_fields();
    test_optional_fields();
    test_variant_fields();
    test_include_following();
    test_builtin_types();
    printf("All struct parser tests passed!\n");
    return 0;
}
