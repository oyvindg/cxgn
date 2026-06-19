#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_all_types_fixture_generates_c11_output(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(utils != NULL);
    assert(parser != NULL);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/all_types.h", &err));

    const cxgn_struct_info* info = cxgn_struct_parser_find_struct(parser, "AllTypesConfig");
    assert(info != NULL);
    assert(cxgn_struct_get_field_count(info) == 31);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    assert(gen != NULL);

    cxgn_output* out = cxgn_generate(gen, "fixtures/all_types.yaml", "fixtures/all_types.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "static const AllTypesConfig config =") != NULL);
    assert(strstr(code, ".intVal = 42") != NULL);
    assert(strstr(code, ".u64Val = 10000000000000000000ULL") != NULL);
    assert(strstr(code, ".boolTrue = true") != NULL);
    assert(strstr(code, ".strVal = \"hello cxgn\"") != NULL);
    assert(strstr(code, ".intArray = {.data = _backing_") != NULL);
    assert(strstr(code, ".optIntPresent = {.value = 99, .has_value = true}") != NULL);
    assert(strstr(code, ".optIntAbsent = {.has_value = false}") != NULL);
    assert(strstr(code, ".nested = {") != NULL);
    assert(strstr(code, ".shapeCircle = {") != NULL);
    assert(strstr(code, ".shapeRect = {") != NULL);
    assert(strstr(code, "std::") == NULL);
    assert(strstr(code, "constexpr") == NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

int main(void) {
    printf("Running all_types tests...\n");
    test_all_types_fixture_generates_c11_output();
    printf("All all_types tests passed!\n");
    return 0;
}
