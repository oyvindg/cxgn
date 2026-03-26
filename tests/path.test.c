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

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Cases
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_path_basic(void) {
    cg_path* path = cg_path_new();
    assert(path != NULL);

    /* Empty path */
    char* str = cg_path_to_string(path);
    assert(str != NULL);
    assert(strcmp(str, "") == 0);
    free(str);

    /* Single key */
    cg_path_push(path, "config");
    str = cg_path_to_string(path);
    assert(strcmp(str, "config") == 0);
    free(str);

    /* Nested keys */
    cg_path_push(path, "server");
    str = cg_path_to_string(path);
    assert(strcmp(str, "config.server") == 0);
    free(str);

    cg_path_push(path, "port");
    str = cg_path_to_string(path);
    assert(strcmp(str, "config.server.port") == 0);
    free(str);

    cg_path_free(path);
    printf("  ✓ test_path_basic\n");
}

static void test_path_with_index(void) {
    cg_path* path = cg_path_new();
    assert(path != NULL);

    cg_path_push(path, "items");
    cg_path_push_index(path, 0);

    char* str = cg_path_to_string(path);
    assert(strcmp(str, "items[0]") == 0);
    free(str);

    cg_path_push(path, "name");
    str = cg_path_to_string(path);
    assert(strcmp(str, "items[0].name") == 0);
    free(str);

    cg_path_free(path);
    printf("  ✓ test_path_with_index\n");
}

static void test_path_pop(void) {
    cg_path* path = cg_path_new();
    assert(path != NULL);

    cg_path_push(path, "config");
    cg_path_push(path, "server");
    cg_path_push(path, "port");

    char* str = cg_path_to_string(path);
    assert(strcmp(str, "config.server.port") == 0);
    free(str);

    cg_path_pop(path);
    str = cg_path_to_string(path);
    assert(strcmp(str, "config.server") == 0);
    free(str);

    cg_path_pop(path);
    str = cg_path_to_string(path);
    assert(strcmp(str, "config") == 0);
    free(str);

    cg_path_pop(path);
    str = cg_path_to_string(path);
    assert(strcmp(str, "") == 0);
    free(str);

    /* Pop on empty should be safe */
    cg_path_pop(path);
    str = cg_path_to_string(path);
    assert(strcmp(str, "") == 0);
    free(str);

    cg_path_free(path);
    printf("  ✓ test_path_pop\n");
}

static void test_path_complex(void) {
    cg_path* path = cg_path_new();
    assert(path != NULL);

    /* Build complex path: config.servers[2].endpoints[0].url */
    cg_path_push(path, "config");
    cg_path_push(path, "servers");
    cg_path_push_index(path, 2);
    cg_path_push(path, "endpoints");
    cg_path_push_index(path, 0);
    cg_path_push(path, "url");

    char* str = cg_path_to_string(path);
    assert(strcmp(str, "config.servers[2].endpoints[0].url") == 0);
    free(str);

    cg_path_free(path);
    printf("  ✓ test_path_complex\n");
}

static void test_path_large_index(void) {
    cg_path* path = cg_path_new();
    assert(path != NULL);

    cg_path_push(path, "items");
    cg_path_push_index(path, 12345);

    char* str = cg_path_to_string(path);
    assert(strcmp(str, "items[12345]") == 0);
    free(str);

    cg_path_free(path);
    printf("  ✓ test_path_large_index\n");
}

static void test_path_null_handling(void) {
    /* NULL path should be handled gracefully */
    char* str = cg_path_to_string(NULL);
    assert(str != NULL);
    assert(strcmp(str, "") == 0);
    free(str);

    /* NULL key should be ignored */
    cg_path* path = cg_path_new();
    cg_path_push(path, NULL);
    str = cg_path_to_string(path);
    assert(strcmp(str, "") == 0);
    free(str);

    /* NULL free should be safe */
    cg_path_free(NULL);

    cg_path_free(path);
    printf("  ✓ test_path_null_handling\n");
}

static void test_path_relative_to_file(void) {
    char* rel = cg_path_relative_to_file("tests/all_types.gen.hpp",
                                         "tests/fixtures/all_types.hpp");
    assert(rel != NULL);
    assert(strcmp(rel, "fixtures/all_types.hpp") == 0);
    free(rel);

    rel = cg_path_relative_to_file("generated/config.gen.hpp",
                                   "include/project/Config.hpp");
    assert(rel != NULL);
    assert(strcmp(rel, "../include/project/Config.hpp") == 0);
    free(rel);

    rel = cg_path_relative_to_file("config.gen.hpp", "Config.hpp");
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
