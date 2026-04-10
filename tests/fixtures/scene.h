#pragma once

#include <cxgn/macros.h>

typedef struct Vec3 {
    float x;
    float y;
    float z;
} Vec3;

typedef struct Point2d {
    float x;
    float y;
} Point2d;

CXGN_ARRAY_TYPEDEF(Point2d, cxgn_array_Point2d_t)
CXGN_OPTIONAL_TYPEDEF(const char*, cxgn_optional_const_char_ptr_t)

typedef struct SceneConfig {
    const char* name;
    Vec3 background;
    cxgn_array_Point2d_t waypoints;
    cxgn_optional_const_char_ptr_t skybox;
    int maxObjects;
} SceneConfig;
