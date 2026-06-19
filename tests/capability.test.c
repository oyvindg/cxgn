#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>

static void test_yaml_capability_flag_matches_build(void) {
#ifdef CXGN_YAML_SUPPORT
    assert(cxgn_has_yaml());
#else
    assert(!cxgn_has_yaml());
#endif
    printf("  ✓ test_yaml_capability_flag_matches_build\n");
}

#ifndef CXGN_YAML_SUPPORT
static void test_disabled_generator_reports_runtime_error(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, NULL);
    assert(gen != NULL);
    assert(cxgn_generate(gen, "fixtures/simple.yaml", "fixtures/simple.h", &err) == NULL);
    assert(err.code == CXGN_ERR_FEATURE_DISABLED);

    cxgn_error_clear(&err);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_disabled_generator_reports_runtime_error\n");
}
#endif

int main(void) {
    printf("Running capability tests...\n");
    test_yaml_capability_flag_matches_build();
#ifndef CXGN_YAML_SUPPORT
    test_disabled_generator_reports_runtime_error();
#endif
    printf("All capability tests passed!\n");
    return 0;
}
