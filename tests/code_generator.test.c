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

static void test_array_and_nullable_pointer_output(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/scene.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate(gen, "fixtures/scene.yaml", "fixtures/scene.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "typedef struct { const Point2d* data; size_t count; } Point2dArray;") == NULL);
    assert(strstr(code, "typedef struct { Point2d value; bool has_value; } OptionalPoint2d;") == NULL);
    assert(strstr(code, "static Point2d const _backing_") != NULL);
    assert(strstr(code, ".waypoints = {.data = _backing_") != NULL);
    assert(strstr(code, ".skybox = 0") != NULL);
    assert(strstr(code, ".previewPoint = {.has_value = false}") != NULL);
    assert(strstr(code, "static const SpawnArea _backing_") != NULL);
    assert(strstr(code, ".pointTarget = 0") != NULL);
    assert(strstr(code, ".spawnArea = &_backing_") != NULL);
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

static void test_qualified_array_element_types(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/qualified_arrays.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate(gen, "fixtures/qualified_arrays.yaml",
                                     "fixtures/qualified_arrays.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "const const") == NULL);
    assert(strstr(code, "static QualifiedItem const _backing_") != NULL);
    assert(strstr(code, "static qualified_string_t const _backing_") != NULL);
    assert(strstr(code, ".items = {.data = _backing_") != NULL);
    assert(strstr(code, ".names = {.data = _backing_") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_multiline_wrapper_alias_generation(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    const char* header_text =
        "#include <stdbool.h>\n"
        "#include <stddef.h>\n"
        "typedef struct {\n"
        "    const char* key;\n"
        "    double value;\n"
        "} demo_pair;\n"
        "typedef struct {\n"
        "    const demo_pair* data;\n"
        "    size_t count;\n"
        "} demo_pair_array_t;\n"
        "typedef struct {\n"
        "    demo_pair value;\n"
        "    bool has_value;\n"
        "} demo_pair_optional_t;\n"
        "typedef struct demo_root {\n"
        "    demo_pair_array_t pairs;\n"
        "    demo_pair_optional_t maybePair;\n"
        "} demo_root;\n";
    const char* yaml_text =
        "pairs:\n"
        "  - key: alpha\n"
        "    value: 1\n"
        "  - key: beta\n"
        "    value: 2.5\n"
        "maybePair:\n"
        "  key: gamma\n"
        "  value: 3\n";

    assert(cxgn_struct_parser_parse_text(parser, header_text, "fixtures/demo_wrappers.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate_from_yaml_text(gen, yaml_text, "fixtures/demo_wrappers.yaml",
                                                    "fixtures/demo_wrappers.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, "static demo_pair const _backing_") != NULL);
    assert(strstr(code, ".pairs = {.data = _backing_") != NULL);
    assert(strstr(code, ".maybePair = {.value =") != NULL);
    assert(strstr(code, ".has_value = true}") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_enum_string_generation(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    const char* header_text =
        "typedef enum demo_mode {\n"
        "    DEMO_MODE_ALPHA = 0,\n"
        "    DEMO_MODE_BETA = 1,\n"
        "} demo_mode;\n"
        "typedef enum demo_sort_by {\n"
        "    DEMO_SORT_SHARPE = 0,\n"
        "    DEMO_SORT_TOTAL_RETURN = 1,\n"
        "} demo_sort_by;\n"
        "typedef struct demo_enum_root {\n"
        "    demo_mode mode;\n"
        "    demo_sort_by sort_by;\n"
        "} demo_enum_root;\n";
    const char* yaml_text =
        "mode: beta\n"
        "sort_by: total_return\n";

    assert(cxgn_struct_parser_parse_text(parser, header_text, "fixtures/demo_enum.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate_from_yaml_text(gen, yaml_text, "fixtures/demo_enum.yaml",
                                                    "fixtures/demo_enum.h", &err);
    assert(out != NULL);

    const char* code = cxgn_output_get_code(out);
    assert(strstr(code, ".mode = DEMO_MODE_BETA") != NULL);
    assert(strstr(code, ".sort_by = DEMO_SORT_TOTAL_RETURN") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_empty_yaml_generation_reports_error(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.h", &err));

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate_from_yaml_text(gen, "", "empty.yaml", "fixtures/simple.h", &err);
    assert(out == NULL);
    assert(err.code == CXGN_ERR_YAML_ERROR);
    assert(err.message != NULL);
    assert(strstr(err.message, "mapping") != NULL);
    assert(err.path != NULL);
    assert(strcmp(err.path, "empty.yaml") == 0);
    cxgn_error_clear(&err);

    cxgn_generator_free(gen);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_large_yaml_text_generation(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.h", &err));

    const size_t name_len = 12000;
    const char* prefix = "timeout: 42\nname: \"";
    const char* suffix = "\"\nenabled: true\n";
    char* yaml = (char*)malloc(strlen(prefix) + name_len + strlen(suffix) + 1);
    assert(yaml != NULL);
    char* w = yaml;
    memcpy(w, prefix, strlen(prefix));
    w += strlen(prefix);
    memset(w, 'a', name_len);
    w += name_len;
    memcpy(w, suffix, strlen(suffix) + 1);

    cxgn_generator* gen = cxgn_generator_new(parser, utils);
    cxgn_output* out = cxgn_generate_from_yaml_text(gen, yaml, "large.yaml", "fixtures/simple.h", &err);
    assert(out != NULL);
    assert(cxgn_output_get_code_length(out) > name_len);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    free(yaml);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_generate_from_document_preserves_numeric_scalars(void) {
    cxgn_error err = {0};
    cxgn_string_utils* utils = cxgn_string_utils_new();
    cxgn_struct_parser* parser = cxgn_struct_parser_new(utils);
    cxgn_document* doc = cxgn_document_new("simple_doc.yaml");
    cxgn_node* root = cxgn_node_new_object();
    cxgn_generator* gen;
    cxgn_output* out;
    const char* code;

    assert(utils != NULL);
    assert(parser != NULL);
    assert(doc != NULL);
    assert(root != NULL);
    assert(cxgn_struct_parser_parse_file(parser, "fixtures/simple.h", &err));
    assert(cxgn_node_object_append(root, "timeout", cxgn_node_new_integer(42), 1, 1));
    assert(cxgn_node_object_append(root, "name", cxgn_node_new_string("doc", 3), 2, 1));
    assert(cxgn_node_object_append(root, "enabled", cxgn_node_new_bool(true), 3, 1));
    assert(cxgn_document_set_root(doc, root));

    gen = cxgn_generator_new(parser, utils);
    assert(gen != NULL);
    out = cxgn_generate_from_document(gen, doc, "simple_doc.yaml", "fixtures/simple.h", &err);
    assert(out != NULL);

    code = cxgn_output_get_code(out);
    assert(strstr(code, ".timeout = 42") != NULL);

    cxgn_output_free(out);
    cxgn_generator_free(gen);
    cxgn_document_free(doc);
    cxgn_struct_parser_free(parser);
    cxgn_string_utils_free(utils);
}

static void test_document_float_nodes_preserve_numeric_text(void) {
    cxgn_node* node = cxgn_node_new_float(2.5);
    size_t len = 0u;
    const char* raw;

    assert(node != NULL);
    raw = cxgn_node_get_raw_scalar_text(node, &len);
    assert(raw != NULL);
    assert(len == 3u);
    assert(strncmp(raw, "2.5", len) == 0);
    cxgn_node_free(node);
}

int main(void) {
    printf("Running code generator tests...\n");
    test_array_and_nullable_pointer_output();
    test_expression_handler_emits_c_initializer();
    test_expression_handler_validation_failure();
    test_qualified_array_element_types();
    test_multiline_wrapper_alias_generation();
    test_enum_string_generation();
    test_empty_yaml_generation_reports_error();
    test_large_yaml_text_generation();
    test_generate_from_document_preserves_numeric_scalars();
    test_document_float_nodes_preserve_numeric_text();
    printf("All code generator tests passed!\n");
    return 0;
}
