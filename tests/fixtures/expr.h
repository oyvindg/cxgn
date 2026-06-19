#pragma once

typedef const char* cxgn_expr_t;

typedef struct ExprConfig {
    const char* name;
    cxgn_expr_t rule;
} ExprConfig;
