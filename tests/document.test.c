#include "internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_document_tracks_source_and_root(void) {
    cxgn_document* doc = cxgn_document_new("fixtures/example.yaml");
    assert(doc != NULL);
    assert(doc->root == NULL);
    assert(strcmp(doc->source_name, "fixtures/example.yaml") == 0);

    cxgn_node* root = cxgn_node_new_object();
    assert(root != NULL);
    cxgn_node_set_location(root, 1, 1);
    assert(cxgn_document_set_root(doc, root));
    assert(doc->root == root);
    assert(doc->root->line == 1);
    assert(doc->root->column == 1);

    cxgn_document_free(doc);
    printf("  ✓ test_document_tracks_source_and_root\n");
}

static void test_array_preserves_order(void) {
    cxgn_node* array = cxgn_node_new_array();
    assert(array != NULL);

    assert(cxgn_node_array_append(array, cxgn_node_new_integer(10)));
    assert(cxgn_node_array_append(array, cxgn_node_new_integer(20)));
    assert(cxgn_node_array_append(array, cxgn_node_new_integer(30)));

    assert(array->type == CXGN_NODE_ARRAY);
    assert(array->as.array.count == 3);
    assert(array->as.array.items[0]->as.int_value == 10);
    assert(array->as.array.items[1]->as.int_value == 20);
    assert(array->as.array.items[2]->as.int_value == 30);

    cxgn_node_free(array);
    printf("  ✓ test_array_preserves_order\n");
}

static void test_object_preserves_duplicates_and_locations(void) {
    cxgn_node* object = cxgn_node_new_object();
    assert(object != NULL);

    cxgn_node* first = cxgn_node_new_string("alpha", strlen("alpha"));
    cxgn_node* second = cxgn_node_new_string("beta", strlen("beta"));
    assert(first != NULL);
    assert(second != NULL);

    assert(cxgn_node_object_append(object, "value", first, 4, 2));
    assert(cxgn_node_object_append(object, "value", second, 5, 7));

    assert(object->type == CXGN_NODE_OBJECT);
    assert(object->as.object.count == 2);
    assert(strcmp(object->as.object.entries[0].key, "value") == 0);
    assert(strcmp(object->as.object.entries[1].key, "value") == 0);
    assert(object->as.object.entries[0].line == 4);
    assert(object->as.object.entries[0].column == 2);
    assert(object->as.object.entries[1].line == 5);
    assert(object->as.object.entries[1].column == 7);
    assert(strcmp(object->as.object.entries[0].value->as.string.data, "alpha") == 0);
    assert(strcmp(object->as.object.entries[1].value->as.string.data, "beta") == 0);

    cxgn_node_free(object);
    printf("  ✓ test_object_preserves_duplicates_and_locations\n");
}

static void test_document_free_releases_nested_tree(void) {
    cxgn_document* doc = cxgn_document_new("fixtures/nested.yaml");
    assert(doc != NULL);

    cxgn_node* root = cxgn_node_new_object();
    cxgn_node* array = cxgn_node_new_array();
    cxgn_node* nested = cxgn_node_new_object();
    assert(root != NULL);
    assert(array != NULL);
    assert(nested != NULL);

    assert(cxgn_node_array_append(array, cxgn_node_new_bool(true)));
    assert(cxgn_node_array_append(array, cxgn_node_new_float(2.5)));
    assert(cxgn_node_object_append(nested, "leaf", cxgn_node_new_null(), 9, 3));
    assert(cxgn_node_object_append(root, "items", array, 3, 1));
    assert(cxgn_node_object_append(root, "nested", nested, 8, 1));
    assert(cxgn_document_set_root(doc, root));

    cxgn_document_free(doc);
    printf("  ✓ test_document_free_releases_nested_tree\n");
}

static void test_document_to_yaml_roundtrip(void) {
    cxgn_document* doc = cxgn_document_new("fixtures/generated.yaml");
    assert(doc != NULL);

    cxgn_node* root = cxgn_node_new_object();
    cxgn_node* items = cxgn_node_new_array();
    cxgn_node* item = cxgn_node_new_object();
    assert(root != NULL);
    assert(items != NULL);
    assert(item != NULL);

    assert(cxgn_node_object_append(root, "name", cxgn_node_new_string("alpha", 5), 1, 1));
    assert(cxgn_node_object_append(root, "enabled", cxgn_node_new_bool(true), 2, 1));
    assert(cxgn_node_object_append(item, "key", cxgn_node_new_string("beta", 4), 4, 3));
    assert(cxgn_node_object_append(item, "weight", cxgn_node_new_float(2.5), 5, 3));
    assert(cxgn_node_array_append(items, item));
    assert(cxgn_node_object_append(root, "items", items, 3, 1));
    assert(cxgn_document_set_root(doc, root));

    char* yaml = cxgn_document_to_yaml_text(doc);
    assert(yaml != NULL);
    assert(strstr(yaml, "name: \"alpha\"") != NULL);
    assert(strstr(yaml, "enabled: true") != NULL);
    assert(strstr(yaml, "- key: \"beta\"") != NULL);

    cxgn_error err = {0};
    cxgn_document* reparsed = cxgn_document_from_yaml_text(yaml, "fixtures/generated.yaml", &err);
    assert(reparsed != NULL);

    const cxgn_node* reparsed_root = cxgn_document_get_root(reparsed);
    assert(reparsed_root != NULL);
    assert(cxgn_node_get_type(reparsed_root) == CXGN_NODE_OBJECT);
    assert(strcmp(cxgn_node_get_string(cxgn_node_object_find(reparsed_root, "name", 0), NULL), "alpha") == 0);
    {
        bool enabled = false;
        assert(cxgn_node_get_bool(cxgn_node_object_find(reparsed_root, "enabled", 0), &enabled));
        assert(enabled);
    }
    const cxgn_node* reparsed_items = cxgn_node_object_find(reparsed_root, "items", 0);
    assert(reparsed_items != NULL);
    assert(cxgn_node_array_count(reparsed_items) == 1u);

    free(yaml);
    cxgn_document_free(reparsed);
    cxgn_document_free(doc);
    printf("  ✓ test_document_to_yaml_roundtrip\n");
}

static void test_document_serializes_empty_collections_inline(void) {
    cxgn_document* doc = cxgn_document_new("fixtures/empty.yaml");
    cxgn_node* root = cxgn_node_new_object();
    assert(doc != NULL);
    assert(root != NULL);

    assert(cxgn_node_object_append(root, "items", cxgn_node_new_array(), 1, 1));
    assert(cxgn_node_object_append(root, "meta", cxgn_node_new_object(), 2, 1));
    assert(cxgn_document_set_root(doc, root));

    char* yaml = cxgn_document_to_yaml_text(doc);
    assert(yaml != NULL);
    assert(strstr(yaml, "items: []\n") != NULL);
    assert(strstr(yaml, "meta: {}\n") != NULL);

    free(yaml);
    cxgn_document_free(doc);
    printf("  ✓ test_document_serializes_empty_collections_inline\n");
}

static void test_document_serializes_nested_empty_collections_inline(void) {
    cxgn_document* doc = cxgn_document_new("fixtures/nested_empty.yaml");
    cxgn_node* root = cxgn_node_new_object();
    cxgn_node* items = cxgn_node_new_array();
    cxgn_node* item = cxgn_node_new_object();
    assert(doc != NULL);
    assert(root != NULL);
    assert(items != NULL);
    assert(item != NULL);

    assert(cxgn_node_object_append(item, "name", cxgn_node_new_string("alpha", 5), 1, 1));
    assert(cxgn_node_object_append(item, "tags", cxgn_node_new_array(), 2, 1));
    assert(cxgn_node_object_append(item, "meta", cxgn_node_new_object(), 3, 1));
    assert(cxgn_node_array_append(items, item));
    assert(cxgn_node_object_append(root, "items", items, 4, 1));
    assert(cxgn_document_set_root(doc, root));

    char* yaml = cxgn_document_to_yaml_text(doc);
    assert(yaml != NULL);
    assert(strstr(yaml, "  tags: []\n") != NULL);
    assert(strstr(yaml, "  meta: {}\n") != NULL);

    free(yaml);
    cxgn_document_free(doc);
    printf("  ✓ test_document_serializes_nested_empty_collections_inline\n");
}

int main(void) {
    printf("Running document tests...\n");
    test_document_tracks_source_and_root();
    test_array_preserves_order();
    test_object_preserves_duplicates_and_locations();
    test_document_free_releases_nested_tree();
    test_document_to_yaml_roundtrip();
    test_document_serializes_empty_collections_inline();
    test_document_serializes_nested_empty_collections_inline();
    printf("All document tests passed!\n");
    return 0;
}
