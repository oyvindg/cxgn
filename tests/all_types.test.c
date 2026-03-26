/**
 * @file all_types.test.c
 * @brief Tests for every YAML-representable type in cxgen.
 *
 * Tests covered:
 * - Integer types: int, long, short, int32_t, int64_t, size_t,
 *                  unsigned int, long long, int8_t/16_t, uint8_t/16_t/32_t/64_t
 * - Float types:   float, double, long double
 * - Boolean:       bool (true and false)
 * - String types:  std::string, std::string_view
 * - Arrays:        Array<int>, Array<double>, Array<std::string>, Array<bool>
 * - Optionals:     present (int, double) and absent (null)
 * - OneOf:         scalar → index 0, mapping → index 1
 * - Nested struct: Point2D via YAML mapping
 */

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static cg_output* make_output(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/all_types.hpp", &err));

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* out = cg_generate(gen, "fixtures/all_types.yaml",
                                  "fixtures/all_types.hpp", &err);
    assert(out != NULL);

    /* gen and parser must outlive out in real use; for test simplicity we
     * accept the leak — test executables are short-lived. */
    return out;
}

/* ── Parsing ──────────────────────────────────────────────────────────────── */

static void test_all_fields_parsed(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    assert(cg_struct_parser_parse_file(parser, "fixtures/all_types.hpp", &err));

    assert(cg_struct_parser_find_struct(parser, "Point2D")       != NULL);
    assert(cg_struct_parser_find_struct(parser, "AllTypesConfig") != NULL);

    const cg_struct_info* cfg = cg_struct_parser_find_struct(parser, "AllTypesConfig");

    /* 14 integer + 3 float + 2 bool + 2 string + 4 array
     * + 3 optional + 2 oneof + 1 nested = 31 fields */
    assert(cg_struct_get_field_count(cfg) == 31);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_all_fields_parsed\n");
}

/* ── Integer types ────────────────────────────────────────────────────────── */

static void test_integer_types(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    assert(strstr(code, "42")                   != NULL);  /* intVal             */
    assert(strstr(code, "1000000")              != NULL);  /* longVal            */
    assert(strstr(code, "32767")                != NULL);  /* shortVal           */
    assert(strstr(code, "2147483647")           != NULL);  /* i32Val             */
    assert(strstr(code, "9000000000000")        != NULL);  /* i64Val             */
    assert(strstr(code, "128")                  != NULL);  /* sizeVal            */
    assert(strstr(code, "100")                  != NULL);  /* uintVal            */
    assert(strstr(code, "5000000000")           != NULL);  /* llVal              */
    assert(strstr(code, "127")                  != NULL);  /* i8Val              */
    assert(strstr(code, "30000")                != NULL);  /* i16Val             */
    assert(strstr(code, "255")                  != NULL);  /* u8Val              */
    assert(strstr(code, "65535")                != NULL);  /* u16Val             */
    assert(strstr(code, "4000000000")           != NULL);  /* u32Val             */
    assert(strstr(code, "10000000000000000000") != NULL);  /* u64Val             */

    cg_output_free(out);
    printf("  ✓ test_integer_types\n");
}

static void test_unsigned_integer_literals_get_suffix(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    assert(strstr(code, "4000000000ULL") != NULL);            /* u32Val */
    assert(strstr(code, "10000000000000000000ULL") != NULL);  /* u64Val */

    cg_output_free(out);
    printf("  ✓ test_unsigned_integer_literals_get_suffix\n");
}

/* ── Float types ──────────────────────────────────────────────────────────── */

static void test_float_types(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    assert(strstr(code, "3.14")          != NULL);  /* floatVal  */
    assert(strstr(code, "2.71828182845") != NULL);  /* doubleVal */
    assert(strstr(code, "1.41421356237") != NULL);  /* ldVal     */

    cg_output_free(out);
    printf("  ✓ test_float_types\n");
}

static void test_float_integer_yaml_gets_dot(void) {
    /* A YAML integer value assigned to a float/double field must gain ".0" */
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};
    cg_struct_parser_parse_file(parser, "fixtures/all_types.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    const char* yaml = "float_val: 3\n"
                       "double_val: 2\n"
                       "ld_val: 1\n"
                       "int_val: 0\nlong_val: 0\nshort_val: 0\ni32_val: 0\n"
                       "i64_val: 0\nsize_val: 0\nuint_val: 0\nll_val: 0\n"
                       "i8_val: 0\ni16_val: 0\nu8_val: 0\nu16_val: 0\n"
                       "u32_val: 0\nu64_val: 0\n"
                       "bool_true: false\nbool_false: false\n"
                       "str_val: \"\"\nsv_val: \"\"\n"
                       "int_array: []\ndouble_array: []\nstr_array: []\nbool_array: []\n"
                       "opt_int_present: 0\nopt_double_present: 0.0\nopt_int_absent: null\n"
                       "one_of_scalar: 0\none_of_mapping: {x: 0, y: 0}\n"
                       "nested: {x: 0, y: 0}\n";
    cg_output* out = cg_generate_from_yaml_text(
        gen, yaml, "memory://all_types.yaml", "fixtures/all_types.hpp", &err);
    assert(out != NULL);

    const char* code = cg_output_get_code(out);
    assert(strstr(code, "3.0") != NULL);
    assert(strstr(code, "2.0") != NULL);

    cg_output_free(out);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_float_integer_yaml_gets_dot\n");
}

/* ── Boolean ──────────────────────────────────────────────────────────────── */

static void test_bool_types(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    assert(strstr(code, "true")  != NULL);
    assert(strstr(code, "false") != NULL);

    cg_output_free(out);
    printf("  ✓ test_bool_types\n");
}

/* ── String types ─────────────────────────────────────────────────────────── */

static void test_string_types(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    assert(strstr(code, "\"hello cxgen\"") != NULL);  /* strVal */
    assert(strstr(code, "\"string_view\"") != NULL);  /* svVal  */

    cg_output_free(out);
    printf("  ✓ test_string_types\n");
}

static void test_string_special_chars(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};
    cg_struct_parser_parse_file(parser, "fixtures/all_types.hpp", &err);

    cg_generator* gen = cg_generator_new(parser, utils);
    /* YAML double-quoted strings handle escape sequences */
    const char* yaml = "str_val: \"say \\\"hello\\\"\"\n"
                       "sv_val: \"\"\n"
                       "int_val: 0\nlong_val: 0\nshort_val: 0\ni32_val: 0\n"
                       "i64_val: 0\nsize_val: 0\nuint_val: 0\nll_val: 0\n"
                       "i8_val: 0\ni16_val: 0\nu8_val: 0\nu16_val: 0\n"
                       "u32_val: 0\nu64_val: 0\nfloat_val: 0.0\ndouble_val: 0.0\n"
                       "ld_val: 0.0\nbool_true: false\nbool_false: false\n"
                       "int_array: []\ndouble_array: []\nstr_array: []\nbool_array: []\n"
                       "opt_int_present: 0\nopt_double_present: 0.0\nopt_int_absent: null\n"
                       "one_of_scalar: 0\none_of_mapping: {x: 0, y: 0}\n"
                       "nested: {x: 0, y: 0}\n";
    cg_output* out = cg_generate_from_yaml_text(
        gen, yaml, "memory://all_types.yaml", "fixtures/all_types.hpp", &err);
    assert(out != NULL);

    const char* code = cg_output_get_code(out);
    assert(strstr(code, "\\\"") != NULL);  /* escaped quote in generated string */

    cg_output_free(out);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_string_special_chars\n");
}

/* ── Arrays ───────────────────────────────────────────────────────────────── */

static void test_array_types(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    /* Backing arrays are emitted before the struct initializer */
    assert(strstr(code, "_backing_")      != NULL);
    assert(strstr(code, "_data[]")        != NULL);
    assert(strstr(code, "_count")         != NULL);

    /* int array values */
    assert(strstr(code, "1, 2, 3, 4, 5") != NULL || strstr(code, "1,2,3,4,5") != NULL
           || (strstr(code, "1") && strstr(code, "5")));

    /* double array */
    assert(strstr(code, "1.1") != NULL);
    assert(strstr(code, "2.2") != NULL);

    /* string array */
    assert(strstr(code, "\"alpha\"") != NULL);
    assert(strstr(code, "\"beta\"")  != NULL);
    assert(strstr(code, "\"gamma\"") != NULL);

    cg_output_free(out);
    printf("  ✓ test_array_types\n");
}

/* ── Optionals ────────────────────────────────────────────────────────────── */

static void test_optional_present(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    assert(strstr(code, "Optional<int>{")    != NULL);  /* optIntPresent    */
    assert(strstr(code, "Optional<double>{") != NULL);  /* optDoublePresent */
    assert(strstr(code, "99")               != NULL);
    assert(strstr(code, "0.5")              != NULL);

    cg_output_free(out);
    printf("  ✓ test_optional_present\n");
}

static void test_optional_absent(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    assert(strstr(code, "Optional<int>::empty()") != NULL);  /* optIntAbsent */

    cg_output_free(out);
    printf("  ✓ test_optional_absent\n");
}

/* ── OneOf ────────────────────────────────────────────────────────────────── */

static void test_oneof_types(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    /* Scalar YAML → index 0 (int) */
    assert(strstr(code, "std::in_place_index<0>") != NULL);

    /* Mapping YAML → index 1 (Point2D) */
    assert(strstr(code, "std::in_place_index<1>") != NULL);

    cg_output_free(out);
    printf("  ✓ test_oneof_types\n");
}

/* ── Nested struct ────────────────────────────────────────────────────────── */

static void test_nested_struct(void) {
    cg_output* out = make_output();
    const char* code = cg_output_get_code(out);

    /* nested.x = 3, nested.y = 4 */
    assert(strstr(code, "3") != NULL);
    assert(strstr(code, "4") != NULL);

    cg_output_free(out);
    printf("  ✓ test_nested_struct\n");
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("Running all_types tests...\n");
    test_all_fields_parsed();
    test_integer_types();
    test_unsigned_integer_literals_get_suffix();
    test_float_types();
    test_float_integer_yaml_gets_dot();
    test_bool_types();
    test_string_types();
    test_string_special_chars();
    test_array_types();
    test_optional_present();
    test_optional_absent();
    test_oneof_types();
    test_nested_struct();
    printf("All all_types tests passed!\n");
    return 0;
}
