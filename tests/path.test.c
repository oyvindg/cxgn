/**
 * @file path.test.c
 * @brief Tests for YAML path tracking utilities.
 *
 * Tests covered:
 * - Path construction (push/pop)
 * - Key and index segments
 * - String formatting
 * - Edge cases
 */

#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Cases
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_path_basic(void) {
    cxgn_path* path = cxgn_path_new();
    assert(path != NULL);

    /* Empty path */
    char* str = cxgn_path_to_string(path);
    assert(str != NULL);
    assert(strcmp(str, "") == 0);
    free(str);

    /* Single key */
    cxgn_path_push(path, "config");
    str = cxgn_path_to_string(path);
    assert(strcmp(str, "config") == 0);
    free(str);

    /* Nested keys */
    cxgn_path_push(path, "server");
    str = cxgn_path_to_string(path);
    assert(strcmp(str, "config.server") == 0);
    free(str);

    cxgn_path_push(path, "port");
    str = cxgn_path_to_string(path);
    assert(strcmp(str, "config.server.port") == 0);
    free(str);

    cxgn_path_free(path);
    printf("  ✓ test_path_basic\n");
}

static void test_path_with_index(void) {
    cxgn_path* path = cxgn_path_new();
    assert(path != NULL);

    cxgn_path_push(path, "items");
    cxgn_path_push_index(path, 0);

    char* str = cxgn_path_to_string(path);
    assert(strcmp(str, "items[0]") == 0);
    free(str);

    cxgn_path_push(path, "name");
    str = cxgn_path_to_string(path);
    assert(strcmp(str, "items[0].name") == 0);
    free(str);

    cxgn_path_free(path);
    printf("  ✓ test_path_with_index\n");
}

static void test_path_pop(void) {
    cxgn_path* path = cxgn_path_new();
    assert(path != NULL);

    cxgn_path_push(path, "config");
    cxgn_path_push(path, "server");
    cxgn_path_push(path, "port");

    char* str = cxgn_path_to_string(path);
    assert(strcmp(str, "config.server.port") == 0);
    free(str);

    cxgn_path_pop(path);
    str = cxgn_path_to_string(path);
    assert(strcmp(str, "config.server") == 0);
    free(str);

    cxgn_path_pop(path);
    str = cxgn_path_to_string(path);
    assert(strcmp(str, "config") == 0);
    free(str);

    cxgn_path_pop(path);
    str = cxgn_path_to_string(path);
    assert(strcmp(str, "") == 0);
    free(str);

    /* Pop on empty should be safe */
    cxgn_path_pop(path);
    str = cxgn_path_to_string(path);
    assert(strcmp(str, "") == 0);
    free(str);

    cxgn_path_free(path);
    printf("  ✓ test_path_pop\n");
}

static void test_path_complex(void) {
    cxgn_path* path = cxgn_path_new();
    assert(path != NULL);

    /* Build complex path: config.servers[2].endpoints[0].url */
    cxgn_path_push(path, "config");
    cxgn_path_push(path, "servers");
    cxgn_path_push_index(path, 2);
    cxgn_path_push(path, "endpoints");
    cxgn_path_push_index(path, 0);
    cxgn_path_push(path, "url");

    char* str = cxgn_path_to_string(path);
    assert(strcmp(str, "config.servers[2].endpoints[0].url") == 0);
    free(str);

    cxgn_path_free(path);
    printf("  ✓ test_path_complex\n");
}

static void test_path_large_index(void) {
    cxgn_path* path = cxgn_path_new();
    assert(path != NULL);

    cxgn_path_push(path, "items");
    cxgn_path_push_index(path, 12345);

    char* str = cxgn_path_to_string(path);
    assert(strcmp(str, "items[12345]") == 0);
    free(str);

    cxgn_path_free(path);
    printf("  ✓ test_path_large_index\n");
}

static void test_path_null_handling(void) {
    /* NULL path should be handled gracefully */
    char* str = cxgn_path_to_string(NULL);
    assert(str != NULL);
    assert(strcmp(str, "") == 0);
    free(str);

    /* NULL key should be ignored */
    cxgn_path* path = cxgn_path_new();
    cxgn_path_push(path, NULL);
    str = cxgn_path_to_string(path);
    assert(strcmp(str, "") == 0);
    free(str);

    /* NULL free should be safe */
    cxgn_path_free(NULL);

    cxgn_path_free(path);
    printf("  ✓ test_path_null_handling\n");
}

static void test_path_relative_to_file(void) {
    char* rel = cxgn_path_relative_to_file("tests/all_types.gen.hpp",
                                         "tests/fixtures/all_types.hpp");
    assert(rel != NULL);
    assert(strcmp(rel, "fixtures/all_types.hpp") == 0);
    free(rel);

    rel = cxgn_path_relative_to_file("generated/config.gen.hpp",
                                   "include/project/Config.hpp");
    assert(rel != NULL);
    assert(strcmp(rel, "../include/project/Config.hpp") == 0);
    free(rel);

    rel = cxgn_path_relative_to_file("config.gen.hpp", "Config.hpp");
    assert(rel != NULL);
    assert(strcmp(rel, "Config.hpp") == 0);
    free(rel);

    printf("  ✓ test_path_relative_to_file\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("Running path tests...\n");
    test_path_basic();
    test_path_with_index();
    test_path_pop();
    test_path_complex();
    test_path_large_index();
    test_path_null_handling();
    test_path_relative_to_file();
    printf("All path tests passed!\n");
    return 0;
}
