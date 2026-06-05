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

typedef struct SpawnArea {
    Point2d center;
    float radius;
} SpawnArea;

CXGN_ARRAY_TYPEDEF(Point2d, Point2dArray)
CXGN_OPTIONAL_TYPEDEF(Point2d, OptionalPoint2d)

typedef struct SceneConfig {
    const char* name;
    Vec3 background;
    Point2dArray waypoints;
    const char* skybox;
    OptionalPoint2d previewPoint;
    const Point2d* pointTarget;
    const SpawnArea* spawnArea;
    int maxObjects;
} SceneConfig;
