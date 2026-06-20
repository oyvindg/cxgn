/**
 * @file map.test.c
 * @brief CXGN_MAP_TYPEDEF — a YAML mapping with arbitrary keys generates a
 *        keyed array (key -> first field, scalar value -> second field).
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_map_typedef_generates_keyed_array(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(utils && parser);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/map.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    assert(gen);
    cxgn_generator_set_root_struct(gen, "Strategy");

    cxgn_output* out = cxgn_generate(gen, "fixtures/map.yaml", "fixtures/map.h", &err);
    assert(out != NULL);
    const char* code = cxgn_output_get_code(out);

    /* Each `key: value` becomes an Expr: key -> .name, scalar value -> .expr,
     * preserving order, exposed as {.data, .count}. */
    assert(strstr(code, ".name = \"trend\"") != NULL);
    assert(strstr(code, ".expr = \"close > ema\"") != NULL);
    assert(strstr(code, ".name = \"entry\"") != NULL);
    assert(strstr(code, ".expr = \"trend and rsi > 50\"") != NULL);
    assert(strstr(code, ".expressions = {.data = ") != NULL);
    assert(strstr(code, ".count = 2}") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_string_utils_free(utils);
    printf("  map_typedef keyed-array OK\n");
}

static void test_generate_file_writes_complete_header(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/map.h", &err));
    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_generator_set_root_struct(gen, "Strategy");

    const char* out_path = "map_generated_test.gen.h";
    assert(cxgn_generate_file(gen, "fixtures/map.yaml", "fixtures/map.h",
                              out_path, "fixtures/map.h", NULL, &err));

    FILE* f = fopen(out_path, "rb");
    assert(f);
    char buf[4096] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    remove(out_path);
    assert(n > 0);

    /* Complete, self-contained header: guard + includes + schema + body. */
    assert(strstr(buf, "#ifndef ") != NULL);
    assert(strstr(buf, "#include <stddef.h>") != NULL);
    assert(strstr(buf, "#include \"fixtures/map.h\"") != NULL);
    assert(strstr(buf, "static const Strategy config") != NULL);
    assert(strstr(buf, ".name = \"trend\"") != NULL);
    assert(strstr(buf, "#endif") != NULL);

    cxgn_generator_free(gen);
    cxgn_string_utils_free(utils);
    printf("  generate_file complete-header OK\n");
}

int main(void) {
    printf("map tests:\n");
    test_map_typedef_generates_keyed_array();
    test_generate_file_writes_complete_header();
    printf("All map tests passed.\n");
    return 0;
}
