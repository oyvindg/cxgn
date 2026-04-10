#pragma once

#include <cxgn/macros.h>

CXGN_OPTIONAL_TYPEDEF(const char*, cxgn_optional_const_char_ptr_t)
CXGN_OPTIONAL_TYPEDEF(int, cxgn_optional_int_t)

typedef struct OptionalConfig {
    const char* name;
    cxgn_optional_const_char_ptr_t description;
    cxgn_optional_int_t maxItems;
    int minItems;
} OptionalConfig;
