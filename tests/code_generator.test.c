#include <cxgn/cxgn.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_validate_calls = 0;
static int g_generate_calls = 0;

static bool is_expr_field(const char* field_type, void* userdata) {
    (void)userdata;
    return strcmp(field_type, "cxgn_expr_t") == 0;
}

static char* validate_expr_ok(const char* expression, const char* yaml_path, void* userdata) {
    (void)expression;
    (void)yaml_path;
    (void)userdata;
    g_validate_calls++;
    return NULL;
}

static char* validate_expr_fail(const char* expression, const char* yaml_path, void* userdata) {
    (void)expression;
    (void)yaml_path;
    (void)userdata;
    g_validate_calls++;
    char* msg = (char*)malloc(32);
    assert(msg != NULL);
    strcpy(msg, "Expression rejected by test");
    return msg;
}

static char* generate_expr_literal(const char* expression, const char* yaml_path, void* userdata) {
    (void)yaml_path;
    (void)userdata;
    g_generate_calls++;
    size_t len = strlen(expression);
    char* out = (char*)malloc(len + 3);
    assert(out != NULL);
    out[0] = '"';
    memcpy(out + 1, expression, len);
    out[len + 1] = '"';
    out[len + 2] = '\0';
    return out;
}

static void test_array_and_optional_output(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/scene.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate(gen, "fixtures/scene.yaml", "fixtures/scene.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "typedef struct { const Point2d* data; size_t count; } cxgn_array_Point2d_t;") == NULL);
    assert(strstr(code, "typedef struct { const char* value; bool has_value; } cxgn_optional_const_char_ptr_t;") == NULL);
    assert(strstr(code, "static const Point2d _backing_") != NULL);
    assert(strstr(code, ".waypoints = {.data = _backing_") != NULL);
    assert(strstr(code, ".skybox = {.has_value = false}") != NULL);
    assert(strstr(code, "static const SceneConfig config =") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_expression_handler_emits_c_initializer(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/expr.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_expression_handler handler = {
        .is_expression_field = is_expr_field,
        .generate_code = generate_expr_literal,
        .validate = validate_expr_ok,
        .userdata = NULL,
    };
    g_validate_calls = 0;
    g_generate_calls = 0;
    cxgn_generator_set_expression_handler(gen, &handler);

    cxgn_output* out = cxgn_generate(gen, "fixtures/expr.yaml", "fixtures/expr.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(g_validate_calls == 1);
    assert(g_generate_calls == 1);
    assert(strstr(code, ".rule = \"close > ema(20)\"") != NULL);
    assert(strstr(code, "std::") == NULL);
    assert(strstr(code, "static const ExprConfig config =") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_expression_handler_validation_failure(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/expr.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_expression_handler handler = {
        .is_expression_field = is_expr_field,
        .generate_code = generate_expr_literal,
        .validate = validate_expr_fail,
        .userdata = NULL,
    };
    g_validate_calls = 0;
    g_generate_calls = 0;
    cxgn_generator_set_expression_handler(gen, &handler);

    cxgn_output* out = cxgn_generate(gen, "fixtures/expr.yaml", "fixtures/expr.h", &err);
    assert(out == NULL);
    assert(err.code == CXGN_ERR_EXPRESSION_ERROR);
    assert(err.message != NULL);
    assert(strcmp(err.message, "Expression rejected by test") == 0);
    assert(g_validate_calls == 1);
    assert(g_generate_calls == 0);
    cxgn_error_clear(&err);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

int main(void) {
    printf("Running code generator tests...\n");
    test_array_and_optional_output();
    test_expression_handler_emits_c_initializer();
    test_expression_handler_validation_failure();
    printf("All code generator tests passed!\n");
    return 0;
}
