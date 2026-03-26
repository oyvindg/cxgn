/**
 * @file warning_colors.test.c
 * @brief Tests for warning color output in cxgen code generation.
 */

#include <cxgen/cxgen.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char* fixture_path(const char* name) {
    const char* candidates[] = {
        "tests/fixtures",
        "fixtures",
    };
    const char* file = __FILE__;
    const char* slash = strrchr(file, '/');
    const size_t name_len = strlen(name);

    if (slash) {
        const size_t dir_len = (size_t)(slash - file);
        const char* mid = "/fixtures/";
        char* path = (char*)malloc(dir_len + strlen(mid) + name_len + 1);
        assert(path != NULL);

        memcpy(path, file, dir_len);
        memcpy(path + dir_len, mid, strlen(mid));
        memcpy(path + dir_len + strlen(mid), name, name_len + 1);
        if (access(path, F_OK) == 0) {
            return path;
        }
        free(path);
    }

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        const size_t dir_len = strlen(candidates[i]);
        char* path = (char*)malloc(dir_len + 1 + name_len + 1);
        assert(path != NULL);

        memcpy(path, candidates[i], dir_len);
        path[dir_len] = '/';
        memcpy(path + dir_len + 1, name, name_len + 1);
        if (access(path, F_OK) == 0) {
            return path;
        }
        free(path);
    }

    assert(!"fixture file not found");
    return NULL;
}

static char* capture_stderr_from_generate(cg_generator* gen,
                                          const char* yaml_path,
                                          const char* header_path,
                                          cg_error* err,
                                          cg_output** out) {
    fflush(stderr);

    const int saved_stderr = dup(STDERR_FILENO);
    assert(saved_stderr >= 0);

    FILE* tmp = tmpfile();
    assert(tmp != NULL);
    const int tmp_fd = fileno(tmp);
    assert(tmp_fd >= 0);

    const int dup_ok = dup2(tmp_fd, STDERR_FILENO);
    assert(dup_ok >= 0);

    *out = cg_generate(gen, yaml_path, header_path, err);

    fflush(stderr);
    assert(dup2(saved_stderr, STDERR_FILENO) >= 0);
    close(saved_stderr);

    assert(fseek(tmp, 0, SEEK_END) == 0);
    const long len = ftell(tmp);
    assert(len >= 0);
    assert(fseek(tmp, 0, SEEK_SET) == 0);

    char* text = (char*)malloc((size_t)len + 1);
    assert(text != NULL);
    const size_t n = fread(text, 1, (size_t)len, tmp);
    assert(n == (size_t)len);
    text[len] = '\0';
    fclose(tmp);
    return text;
}

static void test_missing_field_is_red_warning(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};
    char* header_path = fixture_path("warning_missing.hpp");
    char* yaml_path = fixture_path("warning_missing.yaml");

    assert(cg_struct_parser_parse_file(parser, header_path, &err));

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = NULL;
    char* stderr_text = capture_stderr_from_generate(
        gen, yaml_path, header_path, &err, &output);

    assert(output != NULL);
    assert(strstr(stderr_text, "\033[1;31mWarning:\033[0m") != NULL);
    assert(strstr(stderr_text, "is missing in YAML") != NULL);
    assert(strstr(stderr_text, "warning_missing.yaml:1") != NULL);
    assert(strstr(stderr_text, "warning_missing.hpp:1") != NULL);

    free(stderr_text);
    free(yaml_path);
    free(header_path);
    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_missing_field_is_red_warning\n");
}

static void test_unknown_yaml_field_is_yellow_warning(void) {
    cg_string_utils* utils = cg_string_utils_new();
    cg_struct_parser* parser = cg_struct_parser_new(utils);
    cg_error err = {0};
    char* header_path = fixture_path("warning_extra.hpp");
    char* yaml_path = fixture_path("warning_extra.yaml");

    assert(cg_struct_parser_parse_file(parser, header_path, &err));

    cg_generator* gen = cg_generator_new(parser, utils);
    cg_output* output = NULL;
    char* stderr_text = capture_stderr_from_generate(
        gen, yaml_path, header_path, &err, &output);

    assert(output != NULL);
    assert(strstr(stderr_text, "\033[1;33mWarning:\033[0m") != NULL);
    assert(strstr(stderr_text, "is not defined in header struct") != NULL);
    assert(strstr(stderr_text, "will be ignored") != NULL);
    assert(strstr(stderr_text, "warning_extra.yaml:1") != NULL);
    assert(strstr(stderr_text, "warning_extra.hpp:1") != NULL);

    free(stderr_text);
    free(yaml_path);
    free(header_path);
    cg_output_free(output);
    cg_generator_free(gen);
    cg_struct_parser_free(parser);
    cg_string_utils_free(utils);
    printf("  ✓ test_unknown_yaml_field_is_yellow_warning\n");
}

int main(void) {
    printf("Running warning color tests...\n");
    test_missing_field_is_red_warning();
    test_unknown_yaml_field_is_yellow_warning();
    printf("All warning color tests passed!\n");
    return 0;
}
