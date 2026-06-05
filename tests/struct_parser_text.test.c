#include <cxgn/cxgn.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_parse_header_text(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};
    const char* header_text =
        "typedef struct demo_nested {\n"
        "    double threshold;\n"
        "} demo_nested;\n"
        "\n"
        "typedef struct demo_config {\n"
        "    const char* name;\n"
        "    int enabled;\n"
        "    demo_nested nested;\n"
        "} demo_config;\n";

    assert(cxgn_struct_parser_parse_text(parser, header_text, "fixtures/demo.h", &err));

    const cxgn_struct_info* root = cxgn_struct_parser_find_struct(parser, "demo_config");
    assert(root != NULL);
    assert(cxgn_struct_get_field_count(root) == 3u);

    const cxgn_field_info* enabled = cxgn_struct_find_field(root, "enabled");
    assert(enabled != NULL);
    assert(strcmp(cxgn_field_get_type(enabled), "int") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_parse_header_text\n");
}

static void test_parse_multiline_wrapper_aliases(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};
    const char* header_text =
        "typedef struct {\n"
        "    const char* key;\n"
        "    double value;\n"
        "} demo_pair;\n"
        "\n"
        "typedef struct {\n"
        "    const demo_pair* data;\n"
        "    size_t count;\n"
        "} demo_pair_array_t;\n"
        "\n"
        "typedef struct {\n"
        "    demo_pair value;\n"
        "    bool has_value;\n"
        "} demo_pair_optional_t;\n"
        "\n"
        "typedef struct demo_root {\n"
        "    demo_pair_array_t pairs;\n"
        "    demo_pair_optional_t maybePair;\n"
        "} demo_root;\n";

    assert(cxgn_struct_parser_parse_text(parser, header_text, "fixtures/demo_wrappers.h", &err));

    const cxgn_struct_info* root = cxgn_struct_parser_find_struct(parser, "demo_root");
    assert(root != NULL);

    const cxgn_field_info* pairs = cxgn_struct_find_field(root, "pairs");
    assert(pairs != NULL);
    assert(cxgn_field_is_array(pairs));
    assert(strcmp(cxgn_field_get_array_element_type(pairs), "const demo_pair") == 0);

    const cxgn_field_info* maybe_pair = cxgn_struct_find_field(root, "maybePair");
    assert(maybe_pair != NULL);
    assert(cxgn_field_is_optional(maybe_pair));
    assert(strcmp(cxgn_field_get_optional_value_type(maybe_pair), "demo_pair") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_parse_multiline_wrapper_aliases\n");
}

static void test_parse_enum_typedefs(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};
    const char* header_text =
        "typedef enum demo_mode {\n"
        "    DEMO_MODE_ALPHA = 0,\n"
        "    DEMO_MODE_BETA = 1,\n"
        "} demo_mode;\n"
        "typedef struct demo_enum_root {\n"
        "    demo_mode mode;\n"
        "} demo_enum_root;\n";

    assert(cxgn_struct_parser_parse_text(parser, header_text, "fixtures/demo_enum.h", &err));
    assert(cxgn_struct_parser_is_builtin_type(parser, "demo_mode"));

    const cxgn_struct_info* root = cxgn_struct_parser_find_struct(parser, "demo_enum_root");
    assert(root != NULL);
    const cxgn_field_info* mode = cxgn_struct_find_field(root, "mode");
    assert(mode != NULL);
    assert(strcmp(cxgn_field_get_type(mode), "demo_mode") == 0);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_parse_enum_typedefs\n");
}

int main(void) {
    printf("Running struct parser text tests...\n");
    test_parse_header_text();
    test_parse_multiline_wrapper_aliases();
    test_parse_enum_typedefs();
    printf("All struct parser text tests passed!\n");
    return 0;
}
