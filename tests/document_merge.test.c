#include <cxgn/cxgn.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_document_merge_deep_object_overlay(void) {
    const char* base_yaml =
        "name: base\n"
        "params:\n"
        "  alpha: 1\n"
        "  nested:\n"
        "    beta: 2\n"
        "expressions:\n"
        "  setup: foo\n";
    const char* overlay_yaml =
        "params:\n"
        "  nested:\n"
        "    gamma: 3\n"
        "  alpha: 10\n"
        "entry: bar\n";
    cxgn_error err = {0};
    cxgn_document* base = cxgn_document_from_yaml_text(base_yaml, "base.yaml", &err);
    cxgn_document* overlay = cxgn_document_from_yaml_text(overlay_yaml, "overlay.yaml", &err);
    cxgn_document* merged;
    const cxgn_node* root;
    const cxgn_node* params;
    const cxgn_node* nested;
    long long alpha = 0;
    long long beta = 0;
    long long gamma = 0;

    assert(base != NULL);
    assert(overlay != NULL);
    merged = cxgn_document_merge(base, overlay);
    assert(merged != NULL);

    root = cxgn_document_get_root(merged);
    assert(root != NULL);
    assert(cxgn_document_get_source_name(merged) != NULL);
    assert(strcmp(cxgn_document_get_source_name(merged), "overlay.yaml") == 0);

    params = cxgn_node_object_find(root, "params", 0u);
    nested = cxgn_node_object_find(params, "nested", 0u);
    assert(cxgn_node_get_integer(cxgn_node_object_find(params, "alpha", 0u), &alpha));
    assert(cxgn_node_get_integer(cxgn_node_object_find(nested, "beta", 0u), &beta));
    assert(cxgn_node_get_integer(cxgn_node_object_find(nested, "gamma", 0u), &gamma));
    assert(alpha == 10);
    assert(beta == 2);
    assert(gamma == 3);
    assert(cxgn_node_object_find(root, "entry", 0u) != NULL);
    assert(cxgn_node_object_find(root, "expressions", 0u) != NULL);

    cxgn_document_free(merged);
    cxgn_document_free(overlay);
    cxgn_document_free(base);
    printf("  ✓ test_document_merge_deep_object_overlay\n");
}

static void test_document_merge_many_applies_overlays_in_order(void) {
    const char* base_yaml =
        "params:\n"
        "  alpha: 1\n"
        "  nested:\n"
        "    beta: 2\n";
    const char* overlay_a_yaml =
        "params:\n"
        "  alpha: 10\n"
        "  nested:\n"
        "    gamma: 3\n";
    const char* overlay_b_yaml =
        "params:\n"
        "  alpha: 20\n"
        "  nested:\n"
        "    delta: 4\n"
        "exit: baz\n";
    cxgn_error err = {0};
    cxgn_document* base = cxgn_document_from_yaml_text(base_yaml, "base.yaml", &err);
    cxgn_document* overlay_a = cxgn_document_from_yaml_text(overlay_a_yaml, "overlay_a.yaml", &err);
    cxgn_document* overlay_b = cxgn_document_from_yaml_text(overlay_b_yaml, "overlay_b.yaml", &err);
    const cxgn_document* docs[] = {base, NULL, overlay_a, overlay_b};
    cxgn_document* merged;
    const cxgn_node* params;
    const cxgn_node* nested;
    long long alpha = 0;
    long long beta = 0;
    long long gamma = 0;
    long long delta = 0;

    assert(base != NULL);
    assert(overlay_a != NULL);
    assert(overlay_b != NULL);

    merged = cxgn_document_merge_many(docs, sizeof(docs) / sizeof(docs[0]));
    assert(merged != NULL);
    assert(strcmp(cxgn_document_get_source_name(merged), "overlay_b.yaml") == 0);

    params = cxgn_node_object_find(cxgn_document_get_root(merged), "params", 0u);
    nested = cxgn_node_object_find(params, "nested", 0u);
    assert(cxgn_node_get_integer(cxgn_node_object_find(params, "alpha", 0u), &alpha));
    assert(cxgn_node_get_integer(cxgn_node_object_find(nested, "beta", 0u), &beta));
    assert(cxgn_node_get_integer(cxgn_node_object_find(nested, "gamma", 0u), &gamma));
    assert(cxgn_node_get_integer(cxgn_node_object_find(nested, "delta", 0u), &delta));
    assert(alpha == 20);
    assert(beta == 2);
    assert(gamma == 3);
    assert(delta == 4);
    assert(cxgn_node_object_find(cxgn_document_get_root(merged), "exit", 0u) != NULL);

    cxgn_document_free(merged);
    cxgn_document_free(overlay_b);
    cxgn_document_free(overlay_a);
    cxgn_document_free(base);
    printf("  ✓ test_document_merge_many_applies_overlays_in_order\n");
}

int main(void) {
    printf("Running document_merge tests...\n");
    test_document_merge_deep_object_overlay();
    test_document_merge_many_applies_overlays_in_order();
    printf("All document_merge tests passed!\n");
    return 0;
}
