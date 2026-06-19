#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CXGN_CLI_PATH
#define CXGN_CLI_PATH "cxgn"
#endif

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    assert(f != NULL);

    assert(fseek(f, 0, SEEK_END) == 0);
    long size = ftell(f);
    assert(size >= 0);
    rewind(f);

    char* buf = (char*)malloc((size_t)size + 1);
    assert(buf != NULL);

    size_t read = fread(buf, 1, (size_t)size, f);
    assert(read == (size_t)size);
    buf[size] = '\0';

    fclose(f);
    return buf;
}

static void test_yaml_flag_accepts_multiple_values(void) {
    const char* output_path = "/tmp/cxgn_cli_batch.gen.h";
    remove(output_path);

    char command[1024];
    int written = snprintf(
        command, sizeof(command),
        "\"%s\" --yaml fixtures/batch/alpha.yaml fixtures/batch/beta.yaml "
        "--header fixtures/simple.h --output %s",
        CXGN_CLI_PATH, output_path);
    assert(written > 0);
    assert((size_t)written < sizeof(command));

    int rc = system(command);
    assert(rc == 0);

    char* generated = read_file(output_path);
    assert(strstr(generated, "fixtures/batch/alpha.yaml") != NULL);
    assert(strstr(generated, "fixtures/batch/beta.yaml") != NULL);
    assert(strstr(generated, "alpha_config") != NULL);
    assert(strstr(generated, "beta_config") != NULL);

    free(generated);
    assert(remove(output_path) == 0);
    printf("  ✓ test_yaml_flag_accepts_multiple_values\n");
}

static void test_strict_mode_fails_on_validation_warning(void) {
    const char* output_path = "/tmp/cxgn_cli_strict.gen.h";
    remove(output_path);

    char command[1024];
    int written = snprintf(
        command, sizeof(command),
        "\"%s\" --yaml fixtures/warning_extra.yaml "
        "--header fixtures/warning_extra.h --output %s --strict",
        CXGN_CLI_PATH, output_path);
    assert(written > 0);
    assert((size_t)written < sizeof(command));

    int rc = system(command);
    assert(rc != 0);

    FILE* generated = fopen(output_path, "rb");
    assert(generated == NULL);

    printf("  ✓ test_strict_mode_fails_on_validation_warning\n");
}

static void test_include_guard_does_not_duplicate_h_suffix(void) {
    const char* output_path = "scene.gen.h";
    remove(output_path);

    char command[1024];
    int written = snprintf(
        command, sizeof(command),
        "\"%s\" --yaml fixtures/simple.yaml "
        "--header fixtures/simple.h --output %s",
        CXGN_CLI_PATH, output_path);
    assert(written > 0);
    assert((size_t)written < sizeof(command));

    int rc = system(command);
    assert(rc == 0);

    char* generated = read_file(output_path);
    assert(strstr(generated, "#ifndef CXGN_SCENE_GEN_H\n") != NULL);
    assert(strstr(generated, "CXGN_SCENE_GEN_H_H") == NULL);

    free(generated);
    assert(remove(output_path) == 0);
    printf("  ✓ test_include_guard_does_not_duplicate_h_suffix\n");
}

static void test_plan_file_runs_multiple_codegen_jobs(void) {
    const char* simple_output = "cli_plan_simple.gen.h";
    const char* batch_output = "cli_plan_batch.gen.h";
    remove(simple_output);
    remove(batch_output);

    char command[1024];
    int written = snprintf(
        command, sizeof(command),
        "\"%s\" --plan fixtures/cli_plan.yaml",
        CXGN_CLI_PATH);
    assert(written > 0);
    assert((size_t)written < sizeof(command));

    int rc = system(command);
    assert(rc == 0);

    char* simple_generated = read_file(simple_output);
    char* batch_generated = read_file(batch_output);
    assert(strstr(simple_generated, "fixtures/simple.yaml") != NULL);
    assert(strstr(simple_generated, "static const SimpleConfig config =") != NULL);
    assert(strstr(batch_generated, "fixtures/batch/alpha.yaml") != NULL);
    assert(strstr(batch_generated, "fixtures/batch/beta.yaml") != NULL);
    assert(strstr(batch_generated, "cli_plan_map") != NULL);

    free(simple_generated);
    free(batch_generated);
    assert(remove(simple_output) == 0);
    assert(remove(batch_output) == 0);
    printf("  ✓ test_plan_file_runs_multiple_codegen_jobs\n");
}

int main(void) {
    printf("Running CLI tests...\n");
    test_yaml_flag_accepts_multiple_values();
    test_strict_mode_fails_on_validation_warning();
    test_include_guard_does_not_duplicate_h_suffix();
    test_plan_file_runs_multiple_codegen_jobs();
    printf("All CLI tests passed!\n");
    return 0;
}
