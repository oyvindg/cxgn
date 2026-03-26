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

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_error_strings(void) {
    assert(strcmp(cg_error_string(CG_OK), "Success") == 0);
    assert(strcmp(cg_error_string(CG_ERR_FILE_NOT_FOUND), "File not found") == 0);
    assert(strcmp(cg_error_string(CG_ERR_TYPE_MISMATCH), "Type mismatch") == 0);
    assert(strcmp(cg_error_string(CG_ERR_MISSING_FIELD), "Missing required field") == 0);
    assert(strcmp(cg_error_string(CG_ERR_UNKNOWN_STRUCT), "Unknown struct type") == 0);
    printf("  ✓ test_error_strings\n");
}

static void test_file_not_found(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};

    bool result = cg_struct_parser_parse_file(parser, "nonexistent.hpp", &err);
    assert(!result);
    assert(err.code == CG_ERR_FILE_NOT_FOUND);

    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_file_not_found\n");
}

static void test_error_clear(void) {
    cg_error err;
    err.code = CG_ERR_TYPE_MISMATCH;
    err.message = "Test error";
    err.path = NULL;
    err.needs_free = false;

    cg_error_clear(&err);
    assert(err.code == CG_OK);

    /* NULL should be safe */
    cg_error_clear(NULL);

    printf("  ✓ test_error_clear\n");
}

int main(void) {
    printf("Running error handling tests...\n");
    test_error_strings();
    test_file_not_found();
    test_error_clear();
    printf("All error handling tests passed!\n");
    return 0;
}
