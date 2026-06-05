/**
 * @file errors.test.c
 * @brief Tests for error handling.
 *
 * Tests covered:
 * - Error message formatting
 * - Type mismatch errors
 * - File not found errors
 * - Path reporting in errors
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_error_strings(void) {
    assert(strcmp(cxgn_error_string(CXGN_OK), "Success") == 0);
    assert(strcmp(cxgn_error_string(CXGN_ERR_FILE_NOT_FOUND), "File not found") == 0);
    assert(strcmp(cxgn_error_string(CXGN_ERR_TYPE_MISMATCH), "Type mismatch") == 0);
    assert(strcmp(cxgn_error_string(CXGN_ERR_MISSING_FIELD), "Missing required field") == 0);
    assert(strcmp(cxgn_error_string(CXGN_ERR_UNKNOWN_STRUCT), "Unknown struct type") == 0);
    printf("  ✓ test_error_strings\n");
}

static void test_file_not_found(void) {
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_error err = {0};

    bool result = cxgn_struct_parser_parse_file(parser, "nonexistent.h", &err);
    assert(!result);
    assert(err.code == CXGN_ERR_FILE_NOT_FOUND);

    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_file_not_found\n");
}

static void test_error_clear(void) {
    cxgn_error err;
    err.code = CXGN_ERR_TYPE_MISMATCH;
    err.message = "Test error";
    err.path = NULL;
    err.needs_free = false;

    cxgn_error_clear(&err);
    assert(err.code == CXGN_OK);

    /* NULL should be safe */
    cxgn_error_clear(NULL);

    printf("  ✓ test_error_clear\n");
}

static void test_root_type_error_reports_source_location(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(utils != NULL);
    assert(parser != NULL);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    assert(gen != NULL);

    cxgn_output* out = cxgn_generate_from_yaml_text(
        gen,
        "- 1\n- 2\n",
        "root_array.yaml",
        "fixtures/simple.h",
        &err);
    assert(out == NULL);
    assert(err.code == CXGN_ERR_YAML_ERROR);
    assert(err.message != NULL);
    assert(strstr(err.message, "mapping") != NULL);
    assert(err.path != NULL);
    assert(strcmp(err.path, "root_array.yaml") == 0);
    assert(err.line == 1);
    assert(err.column == 1);
    cxgn_error_clear(&err);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
    printf("  ✓ test_root_type_error_reports_source_location\n");
}

int main(void) {
    printf("Running error handling tests...\n");
    test_error_strings();
    test_file_not_found();
    test_error_clear();
    test_root_type_error_reports_source_location();
    printf("All error handling tests passed!\n");
    return 0;
}
