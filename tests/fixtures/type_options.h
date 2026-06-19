#pragma once

#include <cxgn/macros.h>

CXGN_ARRAY_TYPEDEF(int, Vec_int)
CXGN_OPTIONAL_TYPEDEF(int, Maybe_int)

typedef struct TypeOptionsConfig {
    Vec_int values;
    Maybe_int max_items;
} TypeOptionsConfig;
