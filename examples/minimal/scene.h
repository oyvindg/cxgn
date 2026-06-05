#pragma once

#include <cxgn/macros.h>

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
    Point2dArray waypoints;
    const char* skybox;
    OptionalPoint2d previewPoint;
    const Point2d* pointTarget;
    const SpawnArea* spawnArea;
} SceneConfig;
