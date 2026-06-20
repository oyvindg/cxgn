#include <cxgn/macros.h>

typedef struct Expr {
    const char* name;
    const char* expr;
} Expr;

CXGN_MAP_TYPEDEF(Expr, ExprMap)

typedef struct Strategy {
    const char* title;
    ExprMap expressions;
} Strategy;
