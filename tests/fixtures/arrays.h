#pragma once

#include <cxgn/macros.h>

CXGN_ARRAY_TYPEDEF(int, cxgn_array_int_t)

typedef struct ItemConfig {
    int id;
    const char* label;
} ItemConfig;

CXGN_ARRAY_TYPEDEF(ItemConfig, cxgn_array_ItemConfig_t)

typedef struct ArrayConfig {
    cxgn_array_int_t values;
    cxgn_array_ItemConfig_t items;
    const char* name;
} ArrayConfig;
