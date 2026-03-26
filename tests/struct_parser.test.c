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

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_simple_struct(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/simple.hpp", &err));

    const cg_struct_info* info = cg_struct_parser_find_struct(parser, "SimpleConfig");
    assert(info != NULL);
    assert(strcmp(cg_struct_get_name(info), "SimpleConfig") == 0);

    /* Check fields */
    assert(cg_struct_get_field_count(info) == 3);

    const cg_field_info* field = cg_struct_get_field(info, 0);
    assert(strcmp(cg_field_get_name(field), "timeout") == 0);
    assert(strcmp(cg_field_get_type(field), "int") == 0);

    field = cg_struct_get_field(info, 1);
    assert(strcmp(cg_field_get_name(field), "name") == 0);
    assert(strcmp(cg_field_get_type(field), "std::string") == 0);

    field = cg_struct_get_field(info, 2);
    assert(strcmp(cg_field_get_name(field), "enabled") == 0);
    assert(strcmp(cg_field_get_type(field), "bool") == 0);
    assert(strcmp(cg_field_get_default(field), "true") == 0);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_simple_struct\n");
}

static void test_array_fields(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/arrays.hpp", &err));

    const cg_struct_info* info = cg_struct_parser_find_struct(parser, "ArrayConfig");
    assert(info != NULL);

    const cg_field_info* field = cg_struct_find_field(info, "values");
    assert(field != NULL);
    assert(cg_field_is_array(field));
    assert(strcmp(cg_field_get_array_element_type(field), "int") == 0);

    field = cg_struct_find_field(info, "items");
    assert(field != NULL);
    assert(cg_field_is_array(field));
    assert(strcmp(cg_field_get_array_element_type(field), "ItemConfig") == 0);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_array_fields\n");
}

static void test_optional_fields(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/optionals.hpp", &err));

    const cg_struct_info* info = cg_struct_parser_find_struct(parser, "OptionalConfig");
    assert(info != NULL);

    const cg_field_info* field = cg_struct_find_field(info, "description");
    assert(field != NULL);
    assert(cg_field_is_optional(field));
    assert(strcmp(cg_field_get_optional_value_type(field), "std::string") == 0);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_optional_fields\n");
}

static void test_oneof_fields(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/oneof.hpp", &err));

    const cg_struct_info* info = cg_struct_parser_find_struct(parser, "OneOfConfig");
    assert(info != NULL);

    const cg_field_info* field = cg_struct_find_field(info, "rules");
    assert(field != NULL);
    assert(cg_field_is_array(field));
    assert(strcmp(cg_field_get_array_element_type(field), "OneOf<std::string, RuleGroup>") == 0);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_oneof_fields\n");
}

static void test_include_following(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    /* main.hpp includes types.hpp */
    assert(cg_struct_parser_parse_file(parser, "fixtures/main.hpp", &err));

    /* Should find structs from both files */
    assert(cg_struct_parser_find_struct(parser, "MainConfig") != NULL);
    assert(cg_struct_parser_find_struct(parser, "SubConfig") != NULL);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_include_following\n");
}

static void test_builtin_types(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);

    assert(cg_struct_parser_is_builtin_type(parser, "int"));
    assert(cg_struct_parser_is_builtin_type(parser, "double"));
    assert(cg_struct_parser_is_builtin_type(parser, "bool"));
    assert(cg_struct_parser_is_builtin_type(parser, "std::string"));
    assert(cg_struct_parser_is_builtin_type(parser, "size_t"));

    assert(!cg_struct_parser_is_builtin_type(parser, "MyCustomType"));
    assert(!cg_struct_parser_is_builtin_type(parser, "ServerConfig"));

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_builtin_types\n");
}

int main(void) {
    printf("Running struct parser tests...\n");
    test_simple_struct();
    test_array_fields();
    test_optional_fields();
    test_oneof_fields();
    test_include_following();
    test_builtin_types();
    printf("All struct parser tests passed!\n");
    return 0;
}
