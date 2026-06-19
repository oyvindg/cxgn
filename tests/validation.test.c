#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int warnings;
    int errors;
    cxgn_error_code last_code;
    char last_path[256];
    char last_message[256];
} diag_collector;

static void collect_diag(cxgn_diagnostic_level level,
                         const cxgn_error* diagnostic,
                         void* userdata) {
    diag_collector* collector = (diag_collector*)userdata;
    assert(collector != NULL);
    assert(diagnostic != NULL);

    if (level == CXGN_DIAGNOSTIC_WARNING) collector->warnings++;
    if (level == CXGN_DIAGNOSTIC_ERROR) collector->errors++;
    collector->last_code = diagnostic->code;

    if (diagnostic->path) {
        snprintf(collector->last_path, sizeof(collector->last_path), "%s", diagnostic->path);
    } else {
        collector->last_path[0] = '\0';
    }

    if (diagnostic->message) {
        snprintf(collector->last_message, sizeof(collector->last_message), "%s", diagnostic->message);
    } else {
        collector->last_message[0] = '\0';
    }
}

static cxgn_generator* make_generator(const char* header_path,
                                      diag_collector* collector,
                                      cxgn_struct_parser** out_parser,
                                      cxgn_string_utils** out_utils) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(utils != NULL);
    assert(parser != NULL);
    assert(cxgn_struct_parser_parse_file(parser, header_path, &err));

    cxgn_generator* gen = cxgn_generator_new(parser, NULL);
    assert(gen != NULL);

    cxgn_validation_options validation;
    cxgn_validation_options_init(&validation);
    validation.diagnostic_fn = collect_diag;
    validation.diagnostic_userdata = collector;
    cxgn_generator_set_validation_options(gen, &validation);

    *out_parser = parser;
    *out_utils = utils;
    return gen;
}

static void test_missing_field_errors_by_default(void) {
    cxgn_error err = {0};
    diag_collector collector = {0};
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_generator("fixtures/warning_missing.h", &collector, &parser, &utils);

    cxgn_output* out = cxgn_generate(gen, "fixtures/warning_missing.yaml", "fixtures/warning_missing.h", &err);
    assert(out == NULL);
    assert(err.code == CXGN_ERR_MISSING_FIELD);
    assert(collector.warnings == 0);
    assert(collector.errors == 1);
    assert(collector.last_code == CXGN_ERR_MISSING_FIELD);
    assert(strstr(collector.last_path, "required_b") != NULL);
    assert(strstr(err.path, "required_b") != NULL);
    cxgn_error_clear(&err);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_missing_field_errors_by_default\n");
}

static void test_multiple_missing_fields_are_reported(void) {
    cxgn_error err = {0};
    diag_collector collector = {0};
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_generator("fixtures/warning_missing.h", &collector, &parser, &utils);

    cxgn_output* out = cxgn_generate_from_yaml_text(
        gen, "{}\n", "fixtures/warning_missing_empty.yaml", "fixtures/warning_missing.h", &err);
    assert(out == NULL);
    assert(err.code == CXGN_ERR_MISSING_FIELD);
    assert(collector.warnings == 0);
    assert(collector.errors == 2);
    assert(collector.last_code == CXGN_ERR_MISSING_FIELD);
    cxgn_error_clear(&err);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_multiple_missing_fields_are_reported\n");
}

static void test_unknown_field_warns_by_default(void) {
    cxgn_error err = {0};
    diag_collector collector = {0};
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_generator("fixtures/warning_extra.h", &collector, &parser, &utils);

    cxgn_output* out = cxgn_generate(gen, "fixtures/warning_extra.yaml", "fixtures/warning_extra.h", &err);
    assert(out != NULL);
    assert(collector.warnings == 1);
    assert(collector.errors == 0);
    assert(collector.last_code == CXGN_ERR_UNKNOWN_FIELD);
    assert(strstr(collector.last_path, "unknown_field") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_unknown_field_warns_by_default\n");
}

static void test_duplicate_key_errors_by_default(void) {
    cxgn_error err = {0};
    diag_collector collector = {0};
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_generator("fixtures/warning_duplicate.h", &collector, &parser, &utils);

    cxgn_output* out = cxgn_generate(gen, "fixtures/warning_duplicate.yaml", "fixtures/warning_duplicate.h", &err);
    assert(out == NULL);
    assert(err.code == CXGN_ERR_DUPLICATE_KEY);
    assert(collector.errors == 1);
    assert(strstr(err.path, "count") != NULL);
    cxgn_error_clear(&err);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_duplicate_key_errors_by_default\n");
}

static void test_strict_mode_promotes_warnings_to_errors(void) {
    cxgn_error err = {0};
    diag_collector collector = {0};
    cxgn_struct_parser* parser;
    cxgn_string_utils* utils;
    cxgn_generator* gen = make_generator("fixtures/warning_extra.h", &collector, &parser, &utils);

    cxgn_generator_set_strict_mode(gen, true);

    cxgn_output* out = cxgn_generate(gen, "fixtures/warning_extra.yaml", "fixtures/warning_extra.h", &err);
    assert(out == NULL);
    assert(err.code == CXGN_ERR_UNKNOWN_FIELD);
    assert(collector.warnings == 0);
    assert(collector.errors == 1);
    assert(strstr(err.path, "unknown_field") != NULL);
    cxgn_error_clear(&err);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_strict_mode_promotes_warnings_to_errors\n");
}

int main(void) {
    printf("Running validation tests...\n");
    test_missing_field_errors_by_default();
    test_multiple_missing_fields_are_reported();
    test_unknown_field_warns_by_default();
    test_duplicate_key_errors_by_default();
    test_strict_mode_promotes_warnings_to_errors();
    printf("All validation tests passed!\n");
    return 0;
}
