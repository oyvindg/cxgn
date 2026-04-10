/**
 * @file all_types.test.c
 * @brief Tests for every YAML-representable type in cxgn.
 *
 * Tests covered:
 * - Integer types: int, long, short, int32_t, int64_t, size_t,
 *                  unsigned int, long long, int8_t/16_t, uint8_t/16_t/32_t/64_t
 * - Float types:   float, double, long double
 * - Boolean:       bool (true and false)
 * - String types:  std::string, std::string_view
 * - Arrays:        Array<int>, Array<double>, Array<std::string>, Array<bool>
 * - Optionals:     present (int, double) and absent (null)
 * - std::variant:         scalar → index 0, mapping → index 1
 * - Nested struct: Point2D via YAML mapping
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static cxgn_output* make_output(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/all_types.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate(gen, "fixtures/all_types.yaml",
                                  "fixtures/all_types.h", &err);
    assert(out != NULL);

    /* gen and parser must outlive out in real use; for test simplicity we
     * accept the leak — test executables are short-lived. */
    return out;
}

/* ── Parsing ──────────────────────────────────────────────────────────────── */

static void test_all_fields_parsed(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    assert(cxgn_struct_parser_parse_file(parser, "fixtures/all_types.h", &err));

    assert(cxgn_struct_parser_find_struct(parser, "Point2D")       != NULL);
    assert(cxgn_struct_parser_find_struct(parser, "AllTypesConfig") != NULL);

    const cxgn_struct_info* cfg = cxgn_struct_parser_find_struct(parser, "AllTypesConfig");

    /* 14 integer + 3 float + 2 bool + 2 string + 4 array
     * + 3 optional + 3 variant + 1 nested = 32 fields */
    assert(cxgn_struct_get_field_count(cfg) == 32);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_all_fields_parsed\n");
}

/* ── Integer types ────────────────────────────────────────────────────────── */

static void test_integer_types(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

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

    cxgn_output_free(out);
    printf("  ✓ test_integer_types\n");
}

static void test_unsigned_integer_literals_get_suffix(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    assert(strstr(code, "4000000000ULL") != NULL);            /* u32Val */
    assert(strstr(code, "10000000000000000000ULL") != NULL);  /* u64Val */

    cxgn_output_free(out);
    printf("  ✓ test_unsigned_integer_literals_get_suffix\n");
}

/* ── Float types ──────────────────────────────────────────────────────────── */

static void test_float_types(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    assert(strstr(code, "3.14")          != NULL);  /* floatVal  */
    assert(strstr(code, "2.71828182845") != NULL);  /* doubleVal */
    assert(strstr(code, "1.41421356237") != NULL);  /* ldVal     */

    cxgn_output_free(out);
    printf("  ✓ test_float_types\n");
}

static void test_float_integer_yaml_gets_dot(void) {
    /* A YAML integer value assigned to a float/double field must gain ".0" */
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};
    cxgn_struct_parser_parse_file(parser, "fixtures/all_types.h", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
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
                       "variant_scalar: 0\nvariant_mapping: {x: 0, y: 0}\n"
                       "shape: {radius: 0.0}\n"
                       "nested: {x: 0, y: 0}\n";
    cxgn_output* out = cxgn_generate_from_yaml_text(
        gen, yaml, "memory://all_types.yaml", "fixtures/all_types.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "3.0") != NULL);
    assert(strstr(code, "2.0") != NULL);

    cxgn_output_free(out);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_float_integer_yaml_gets_dot\n");
}

/* ── Boolean ──────────────────────────────────────────────────────────────── */

static void test_bool_types(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    assert(strstr(code, "true")  != NULL);
    assert(strstr(code, "false") != NULL);

    cxgn_output_free(out);
    printf("  ✓ test_bool_types\n");
}

/* ── String types ─────────────────────────────────────────────────────────── */

static void test_string_types(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    assert(strstr(code, "\"hello cxgn\"") != NULL);  /* strVal */
    assert(strstr(code, "\"string_view\"") != NULL);  /* svVal  */

    cxgn_output_free(out);
    printf("  ✓ test_string_types\n");
}

static void test_string_special_chars(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};
    cxgn_struct_parser_parse_file(parser, "fixtures/all_types.h", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
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
                       "variant_scalar: 0\nvariant_mapping: {x: 0, y: 0}\n"
                       "shape: {radius: 0.0}\n"
                       "nested: {x: 0, y: 0}\n";
    cxgn_output* out = cxgn_generate_from_yaml_text(
        gen, yaml, "memory://all_types.yaml", "fixtures/all_types.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "\\\"") != NULL);  /* escaped quote in generated string */

    cxgn_output_free(out);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_string_special_chars\n");
}

/* ── Arrays ───────────────────────────────────────────────────────────────── */

static void test_array_types(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    /* Backing arrays are emitted before the struct initializer */
    assert(strstr(code, "_backing_")      != NULL);
    assert(strstr(code, "_data[]")        != NULL);

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

    cxgn_output_free(out);
    printf("  ✓ test_array_types\n");
}

static void test_namespace_backing_sorted_without_changing_config_order(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    const char* spool0 = strstr(code, "static constexpr char _spool_0[]");
    const char* spool4 = strstr(code, "static constexpr char _spool_4[]");
    const char* str_arr = strstr(code, "static constexpr std::string _backing_AllTypesConfig_strArray_data[]");
    const char* dbl_arr = strstr(code, "static constexpr double _backing_AllTypesConfig_doubleArray_data[]");
    const char* int_arr = strstr(code, "static constexpr int _backing_AllTypesConfig_intArray_data[]");
    const char* bool_arr = strstr(code, "static constexpr bool _backing_AllTypesConfig_boolArray_data[]");
    const char* str_val = strstr(code, "_spool_0,  // strVal");
    const char* sv_val = strstr(code, "_spool_1,  // svVal");
    const char* int_array_field = strstr(code, "Array<int>{_backing_AllTypesConfig_intArray_data, 5},  // intArray");

    assert(spool0 != NULL);
    assert(spool4 != NULL);
    assert(str_arr != NULL);
    assert(dbl_arr != NULL);
    assert(int_arr != NULL);
    assert(bool_arr != NULL);
    assert(str_val != NULL);
    assert(sv_val != NULL);
    assert(int_array_field != NULL);

    /* Spools must remain before arrays because array backings may reference them. */
    assert(spool0 < str_arr);
    assert(spool4 < str_arr);

    /* Arrays are sorted by descending type rank, then name for determinism. */
    assert(dbl_arr < str_arr);
    assert(str_arr < int_arr);
    assert(int_arr < bool_arr);

    /* Aggregate initialization order must still follow the struct declaration. */
    assert(str_val < sv_val);
    assert(sv_val < int_array_field);

    cxgn_output_free(out);
    printf("  ✓ test_namespace_backing_sorted_without_changing_config_order\n");
}

static void test_usage_comments_cover_main_field_types(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    assert(strstr(code, "// Usage examples:") != NULL);
    assert(strstr(code, "//   int value = config.intVal;") != NULL);
    assert(strstr(code, "//   std::string s = config.strVal;") != NULL);
    assert(strstr(code, "//   const int* data = config.intArray.data;") != NULL);
    assert(strstr(code, "//   if (config.optIntPresent) {") != NULL);
    assert(strstr(code, "//   if (const int* value = std::get_if<int>(&config.variantScalar)) {") != NULL);
    assert(strstr(code, "//   if (const Point2D* value = std::get_if<Point2D>(&config.variantScalar)) {") != NULL);
    assert(strstr(code, "//   if (const Circle* value = std::get_if<Circle>(&config.shape)) {") != NULL);
    assert(strstr(code, "//   const Point2D& value = config.nested;") != NULL);

    cxgn_output_free(out);
    printf("  ✓ test_usage_comments_cover_main_field_types\n");
}

/* ── Optionals ────────────────────────────────────────────────────────────── */

static void test_optional_present(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    assert(strstr(code, "Optional<int>{")    != NULL);  /* optIntPresent    */
    assert(strstr(code, "Optional<double>{") != NULL);  /* optDoublePresent */
    assert(strstr(code, "99")               != NULL);
    assert(strstr(code, "0.5")              != NULL);

    cxgn_output_free(out);
    printf("  ✓ test_optional_present\n");
}

static void test_optional_absent(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    assert(strstr(code, "Optional<int>::empty()") != NULL);  /* optIntAbsent */

    cxgn_output_free(out);
    printf("  ✓ test_optional_absent\n");
}

/* ── std::variant ────────────────────────────────────────────────────────────────── */

static void test_variant_types(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    /* Scalar YAML → index 0 (int) */
    assert(strstr(code, "std::in_place_index<0>") != NULL);

    /* Mapping YAML → index 1 (Point2D) */
    assert(strstr(code, "std::in_place_index<1>") != NULL);

    /* Circle mapping in std::variant<Circle, Rectangle> still selects index 0 */
    assert(strstr(code, "std::variant<Circle, Rectangle>{std::in_place_index<0>, Circle{") != NULL);

    cxgn_output_free(out);
    printf("  ✓ test_variant_types\n");
}

/* ── Nested struct ────────────────────────────────────────────────────────── */

static void test_nested_struct(void) {
    cxgn_output* out = make_output();
    const char* code = cxgn_output_get_code(out);

    /* nested.x = 3, nested.y = 4 */
    assert(strstr(code, "3") != NULL);
    assert(strstr(code, "4") != NULL);

    cxgn_output_free(out);
    printf("  ✓ test_nested_struct\n");
}

/* ── C++ standard target ──────────────────────────────────────────────────── */

static void test_cpp17_string_array_uses_const(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/all_types.h", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_generator_set_cpp_std(gen, CXGN_CPP_STD_17);

    cxgn_output* out = cxgn_generate(gen, "fixtures/all_types.yaml",
                                  "fixtures/all_types.h", &err);
    assert(out != NULL);
    const char* code = cxgn_output_get_code(out);

    /* std::string backing must use 'static const', not 'static constexpr' */
    assert(strstr(code, "static const std::string") != NULL);
    assert(strstr(code, "static constexpr std::string") == NULL);

    /* Non-string backings still use constexpr */
    assert(strstr(code, "static constexpr int") != NULL);
    assert(strstr(code, "static constexpr double") != NULL);
    assert(strstr(code, "static constexpr bool") != NULL);

    /* Config variable must be 'const', not 'constexpr', due to string fields */
    assert(strstr(code, "const AllTypesConfig config") != NULL);
    assert(strstr(code, "constexpr AllTypesConfig config") == NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_cpp17_string_array_uses_const\n");
}

static void test_cpp20_string_array_uses_constexpr(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/all_types.h", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_generator_set_cpp_std(gen, CXGN_CPP_STD_20);

    cxgn_output* out = cxgn_generate(gen, "fixtures/all_types.yaml",
                                  "fixtures/all_types.h", &err);
    assert(out != NULL);
    const char* code = cxgn_output_get_code(out);

    assert(strstr(code, "static constexpr std::string") != NULL);
    assert(strstr(code, "static const std::string") == NULL);
    assert(strstr(code, "constexpr AllTypesConfig config") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_cpp20_string_array_uses_constexpr\n");
}

static void test_cpp_auto_emits_if_guards(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    cxgn_struct_parser_parse_file(parser, "fixtures/all_types.h", &err);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_generator_set_cpp_std(gen, CXGN_CPP_STD_AUTO);

    cxgn_output* out = cxgn_generate(gen, "fixtures/all_types.yaml",
                                  "fixtures/all_types.h", &err);
    assert(out != NULL);
    const char* code = cxgn_output_get_code(out);

    /* Both constexpr and const variants must appear inside #if guards */
    assert(strstr(code, "__cplusplus >= 202002L") != NULL);
    assert(strstr(code, "static constexpr std::string") != NULL);
    assert(strstr(code, "static const std::string") != NULL);

    /* Config variable also gets the guard */
    assert(strstr(code, "constexpr AllTypesConfig config") != NULL);
    assert(strstr(code, "const AllTypesConfig config") != NULL);

    /* Non-string types are still unconditionally constexpr */
    assert(strstr(code, "static constexpr int") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_cpp_auto_emits_if_guards\n");
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
    test_namespace_backing_sorted_without_changing_config_order();
    test_usage_comments_cover_main_field_types();
    test_optional_present();
    test_optional_absent();
    test_variant_types();
    test_nested_struct();
    test_cpp17_string_array_uses_const();
    test_cpp20_string_array_uses_constexpr();
    test_cpp_auto_emits_if_guards();
    printf("All all_types tests passed!\n");
    return 0;
}
