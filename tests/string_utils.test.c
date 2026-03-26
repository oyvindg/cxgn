/**
 * @file string_utils.test.c
 * @brief Tests for string case conversion utilities.
 *
 * Tests covered:
 * - snake_case to camelCase
 * - camelCase to snake_case
 * - PascalCase conversions
 * - Edge cases (empty, single char, numbers)
 */

#include <cxgen/cxgen.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Macros
 * ═══════════════════════════════════════════════════════════════════════════ */

#define TEST_CONVERT(func, input, expected) do { \
    char* result = func(utils, input); \
    assert(result != NULL); \
    if (strcmp(result, expected) != 0) { \
        fprintf(stderr, "FAIL: %s(\"%s\") = \"%s\", expected \"%s\"\n", \
                #func, input, result, expected); \
        exit(1); \
    } \
    free(result); \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Cases
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_to_snake_case(void) {
    cg_string_utils* utils = cg_string_utils_new();
    assert(utils != NULL);

    /* camelCase -> snake_case */
    TEST_CONVERT(cg_to_snake_case, "maxRetryCount", "max_retry_count");
    TEST_CONVERT(cg_to_snake_case, "getValue", "get_value");
    TEST_CONVERT(cg_to_snake_case, "isEnabled", "is_enabled");

    /* PascalCase -> snake_case */
    TEST_CONVERT(cg_to_snake_case, "MaxRetryCount", "max_retry_count");
    TEST_CONVERT(cg_to_snake_case, "ServerConfig", "server_config");

    /* Consecutive uppercase (acronyms) */
    TEST_CONVERT(cg_to_snake_case, "HTTPServer", "http_server");
    TEST_CONVERT(cg_to_snake_case, "getHTTPResponse", "get_http_response");
    TEST_CONVERT(cg_to_snake_case, "XMLParser", "xml_parser");
    TEST_CONVERT(cg_to_snake_case, "parseXML", "parse_xml");

    /* Already snake_case */
    TEST_CONVERT(cg_to_snake_case, "already_snake", "already_snake");
    TEST_CONVERT(cg_to_snake_case, "my_value", "my_value");

    /* Single uppercase letters */
    TEST_CONVERT(cg_to_snake_case, "ID", "id");
    TEST_CONVERT(cg_to_snake_case, "getID", "get_id");
    TEST_CONVERT(cg_to_snake_case, "getUserID", "get_user_id");

    /* Digit followed by uppercase — digit acts as lowercase */
    TEST_CONVERT(cg_to_snake_case, "i32Val",   "i32_val");
    TEST_CONVERT(cg_to_snake_case, "u64Config", "u64_config");
    TEST_CONVERT(cg_to_snake_case, "level3Name", "level3_name");
    TEST_CONVERT(cg_to_snake_case, "i8Val",    "i8_val");
    TEST_CONVERT(cg_to_snake_case, "uint16Max", "uint16_max");

    cg_string_utils_free(utils);
    printf("  ✓ test_to_snake_case\n");
}

static void test_to_camel_case(void) {
    cg_string_utils* utils = cg_string_utils_new();
    assert(utils != NULL);

    /* snake_case -> camelCase */
    TEST_CONVERT(cg_to_camel_case, "max_retry_count", "maxRetryCount");
    TEST_CONVERT(cg_to_camel_case, "get_value", "getValue");
    TEST_CONVERT(cg_to_camel_case, "is_enabled", "isEnabled");

    /* Single word */
    TEST_CONVERT(cg_to_camel_case, "single", "single");
    TEST_CONVERT(cg_to_camel_case, "value", "value");

    /* Already camelCase */
    TEST_CONVERT(cg_to_camel_case, "alreadyCamel", "alreadyCamel");
    TEST_CONVERT(cg_to_camel_case, "getValue", "getValue");

    /* Multiple underscores */
    TEST_CONVERT(cg_to_camel_case, "get__value", "getValue");

    /* Leading underscore (edge case) */
    TEST_CONVERT(cg_to_camel_case, "_private", "private");

    cg_string_utils_free(utils);
    printf("  ✓ test_to_camel_case\n");
}

static void test_to_pascal_case(void) {
    cg_string_utils* utils = cg_string_utils_new();
    assert(utils != NULL);

    /* snake_case -> PascalCase */
    TEST_CONVERT(cg_to_pascal_case, "server_config", "ServerConfig");
    TEST_CONVERT(cg_to_pascal_case, "max_retry_count", "MaxRetryCount");

    /* camelCase -> PascalCase */
    TEST_CONVERT(cg_to_pascal_case, "serverConfig", "ServerConfig");
    TEST_CONVERT(cg_to_pascal_case, "getValue", "GetValue");

    /* Single word */
    TEST_CONVERT(cg_to_pascal_case, "server", "Server");
    TEST_CONVERT(cg_to_pascal_case, "config", "Config");

    /* Already PascalCase */
    TEST_CONVERT(cg_to_pascal_case, "AlreadyPascal", "AlreadyPascal");

    cg_string_utils_free(utils);
    printf("  ✓ test_to_pascal_case\n");
}

static void test_edge_cases(void) {
    cg_string_utils* utils = cg_string_utils_new();
    assert(utils != NULL);

    /* Empty string */
    TEST_CONVERT(cg_to_snake_case, "", "");
    TEST_CONVERT(cg_to_camel_case, "", "");
    TEST_CONVERT(cg_to_pascal_case, "", "");

    /* Single character */
    TEST_CONVERT(cg_to_snake_case, "x", "x");
    TEST_CONVERT(cg_to_camel_case, "x", "x");
    TEST_CONVERT(cg_to_pascal_case, "x", "X");

    TEST_CONVERT(cg_to_snake_case, "X", "x");
    TEST_CONVERT(cg_to_camel_case, "X", "x");
    TEST_CONVERT(cg_to_pascal_case, "X", "X");

    /* Numbers */
    TEST_CONVERT(cg_to_snake_case, "value1", "value1");
    TEST_CONVERT(cg_to_camel_case, "value_1", "value1");
    TEST_CONVERT(cg_to_pascal_case, "value_1", "Value1");

    /* NULL handling */
    char* result = cg_to_snake_case(utils, NULL);
    assert(result == NULL);

    cg_string_utils_free(utils);
    printf("  ✓ test_edge_cases\n");
}

static void test_error_string(void) {
    assert(strcmp(cg_error_string(CG_OK), "Success") == 0);
    assert(strcmp(cg_error_string(CG_ERR_FILE_NOT_FOUND), "File not found") == 0);
    assert(strcmp(cg_error_string(CG_ERR_TYPE_MISMATCH), "Type mismatch") == 0);
    printf("  ✓ test_error_string\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("Running string utils tests...\n");
    test_to_snake_case();
    test_to_camel_case();
    test_to_pascal_case();
    test_edge_cases();
    test_error_string();
    printf("All string utils tests passed!\n");
    return 0;
}
