#pragma once

#include <cxgn/macros.h>

typedef struct Point2d {
    float x;
    float y;
} Point2d;

CXGN_ARRAY_TYPEDEF(Point2d, cxgn_array_Point2d_t)
CXGN_OPTIONAL_TYPEDEF(const char*, cxgn_optional_const_char_ptr_t)

typedef struct SceneConfig {
    const char* name;
    cxgn_array_Point2d_t waypoints;
    cxgn_optional_const_char_ptr_t skybox;
} SceneConfig;
