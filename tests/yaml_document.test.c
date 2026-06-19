#include "internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const cxgn_object_entry* find_object_entry(const cxgn_node* object_node, const char* key, size_t ordinal) {
    size_t seen = 0;
    assert(object_node != NULL);
    assert(object_node->type == CXGN_NODE_OBJECT);

    for (size_t i = 0; i < object_node->as.object.count; i++) {
        const cxgn_object_entry* entry = &object_node->as.object.entries[i];
        if (strcmp(entry->key, key) == 0) {
            if (seen == ordinal) return entry;
            seen++;
        }
    }

    return NULL;
}

static void test_scalar_classification_from_text(void) {
    const char* yaml_text =
        "nothing: null\n"
        "enabled: true\n"
        "threshold: 42\n"
        "ratio: 3.5\n"
        "quoted_number: \"42\"\n"
        "quoted_bool: \"true\"\n";
    cxgn_error err = {0};
    cxgn_document* doc = cxgn_document_from_yaml_text(yaml_text, "memory.yaml", &err);
    assert(doc != NULL);
    assert(doc->root != NULL);
    assert(doc->root->type == CXGN_NODE_OBJECT);
    assert(strcmp(doc->source_name, "memory.yaml") == 0);

    const cxgn_object_entry* nothing = find_object_entry(doc->root, "nothing", 0);
    const cxgn_object_entry* enabled = find_object_entry(doc->root, "enabled", 0);
    const cxgn_object_entry* threshold = find_object_entry(doc->root, "threshold", 0);
    const cxgn_object_entry* ratio = find_object_entry(doc->root, "ratio", 0);
    const cxgn_object_entry* quoted_number = find_object_entry(doc->root, "quoted_number", 0);
    const cxgn_object_entry* quoted_bool = find_object_entry(doc->root, "quoted_bool", 0);
    assert(nothing && enabled && threshold && ratio && quoted_number && quoted_bool);

    assert(nothing->value->type == CXGN_NODE_NULL);
    assert(enabled->value->type == CXGN_NODE_BOOL);
    assert(enabled->value->as.bool_value);
    assert(threshold->value->type == CXGN_NODE_INTEGER);
    assert(threshold->value->as.int_value == 42);
    assert(ratio->value->type == CXGN_NODE_FLOAT);
    assert(ratio->value->as.float_value > 3.49 && ratio->value->as.float_value < 3.51);
    assert(quoted_number->value->type == CXGN_NODE_STRING);
    assert(strcmp(quoted_number->value->as.string.data, "42") == 0);
    assert(quoted_bool->value->type == CXGN_NODE_STRING);
    assert(strcmp(quoted_bool->value->as.string.data, "true") == 0);

    cxgn_document_free(doc);
    printf("  ✓ test_scalar_classification_from_text\n");
}

static void test_duplicate_keys_are_preserved_from_file(void) {
    cxgn_error err = {0};
    cxgn_document* doc = cxgn_document_from_yaml_file("fixtures/warning_duplicate.yaml", &err);
    assert(doc != NULL);
    assert(doc->root != NULL);
    assert(doc->root->type == CXGN_NODE_OBJECT);
    assert(doc->root->as.object.count == 2);

    const cxgn_object_entry* first = find_object_entry(doc->root, "count", 0);
    const cxgn_object_entry* second = find_object_entry(doc->root, "count", 1);
    assert(first != NULL);
    assert(second != NULL);
    assert(first->value->type == CXGN_NODE_INTEGER);
    assert(second->value->type == CXGN_NODE_INTEGER);
    assert(first->value->as.int_value == 1);
    assert(second->value->as.int_value == 2);
    assert(first->line == 1);
    assert(second->line == 2);

    cxgn_document_free(doc);
    printf("  ✓ test_duplicate_keys_are_preserved_from_file\n");
}

static void test_nested_structures_and_locations(void) {
    cxgn_error err = {0};
    cxgn_document* doc = cxgn_document_from_yaml_file("fixtures/scene.yaml", &err);
    assert(doc != NULL);
    assert(doc->root != NULL);
    assert(doc->root->type == CXGN_NODE_OBJECT);
    assert(doc->root->line == 1);
    assert(doc->root->column == 1);

    const cxgn_object_entry* background = find_object_entry(doc->root, "background", 0);
    const cxgn_object_entry* waypoints = find_object_entry(doc->root, "waypoints", 0);
    const cxgn_object_entry* max_objects = find_object_entry(doc->root, "max_objects", 0);
    assert(background && waypoints && max_objects);

    assert(background->value->type == CXGN_NODE_OBJECT);
    assert(waypoints->value->type == CXGN_NODE_ARRAY);
    assert(waypoints->value->as.array.count == 3);
    assert(waypoints->value->as.array.items[0]->type == CXGN_NODE_OBJECT);
    assert(max_objects->value->type == CXGN_NODE_INTEGER);
    assert(max_objects->value->as.int_value == 256);

    const cxgn_object_entry* bg_x = find_object_entry(background->value, "x", 0);
    assert(bg_x != NULL);
    assert(bg_x->value->type == CXGN_NODE_FLOAT);
    assert(bg_x->line == 3);

    cxgn_document_free(doc);
    printf("  ✓ test_nested_structures_and_locations\n");
}

static void test_empty_yaml_returns_empty_document(void) {
    cxgn_error err = {0};
    cxgn_document* doc = cxgn_document_from_yaml_text("", "empty.yaml", &err);
    assert(doc != NULL);
    assert(doc->root == NULL);
    assert(strcmp(doc->source_name, "empty.yaml") == 0);

    cxgn_document_free(doc);
    printf("  ✓ test_empty_yaml_returns_empty_document\n");
}

static void test_invalid_yaml_returns_error(void) {
    cxgn_error err = {0};
    cxgn_document* doc = cxgn_document_from_yaml_text("foo: [1, 2", "broken.yaml", &err);
    assert(doc == NULL);
    assert(err.code == CXGN_ERR_YAML_ERROR);
    assert(err.message != NULL);
    assert(err.path != NULL);
    assert(strcmp(err.path, "broken.yaml") == 0);
    assert(err.line > 0);
    assert(err.column > 0);
    cxgn_error_clear(&err);
    printf("  ✓ test_invalid_yaml_returns_error\n");
}

static void test_non_scalar_mapping_key_reports_location(void) {
    const char* yaml_text =
        "? [alpha, beta]\n"
        ": 3\n";
    cxgn_error err = {0};
    cxgn_document* doc = cxgn_document_from_yaml_text(yaml_text, "compound_key.yaml", &err);
    assert(doc == NULL);
    assert(err.code == CXGN_ERR_PARSE_ERROR);
    assert(err.message != NULL);
    assert(strstr(err.message, "scalar mapping keys") != NULL);
    assert(err.path != NULL);
    assert(strcmp(err.path, "compound_key.yaml") == 0);
    assert(err.line == 1);
    assert(err.column > 0);
    cxgn_error_clear(&err);
    printf("  ✓ test_non_scalar_mapping_key_reports_location\n");
}

int main(void) {
    printf("Running yaml_document tests...\n");
    test_scalar_classification_from_text();
    test_duplicate_keys_are_preserved_from_file();
    test_nested_structures_and_locations();
    test_empty_yaml_returns_empty_document();
    test_invalid_yaml_returns_error();
    test_non_scalar_mapping_key_reports_location();
    printf("All yaml_document tests passed!\n");
    return 0;
}
