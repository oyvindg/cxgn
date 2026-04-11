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

int main(void) {
    printf("Running CLI tests...\n");
    test_yaml_flag_accepts_multiple_values();
    test_strict_mode_fails_on_validation_warning();
    printf("All CLI tests passed!\n");
    return 0;
}
