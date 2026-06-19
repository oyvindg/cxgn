#pragma once

#include <cxgn/macros.h>

typedef const char* cxgn_cstr_t;

CXGN_ARRAY_TYPEDEF(cxgn_cstr_t, cxgn_array_cstr_t)

typedef struct RuleGroup {
    cxgn_array_cstr_t must;
    cxgn_array_cstr_t should;
    cxgn_array_cstr_t must_not;
} RuleGroup;

typedef struct VariantConfig {
    RuleGroup rules;
} VariantConfig;
