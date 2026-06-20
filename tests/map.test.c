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

int main(void) {
    printf("map tests:\n");
    test_map_typedef_generates_keyed_array();
    printf("All map tests passed.\n");
    return 0;
}
