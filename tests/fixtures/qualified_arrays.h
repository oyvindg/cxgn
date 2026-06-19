#pragma once

typedef const char* qualified_string_t;

typedef struct QualifiedItem {
    const char* key;
    double value;
} QualifiedItem;

typedef struct { const QualifiedItem* data; size_t count; } qualified_item_array_t;
typedef struct { const qualified_string_t* data; size_t count; } qualified_string_array_t;

typedef struct QualifiedArrayConfig {
    qualified_item_array_t items;
    qualified_string_array_t names;
} QualifiedArrayConfig;
