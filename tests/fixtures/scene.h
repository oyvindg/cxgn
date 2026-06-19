#pragma once

#include <cxgn/macros.h>

/* Basic vector used by the scene schema. */
typedef struct Vec3 {
    float x;
    float y;
    float z;
} Vec3;

/** 2D point values are valid cxgn nested structs. */
typedef struct Point2d {
    float x; /* horizontal axis */
    float y; /* vertical axis */
} Point2d;

typedef struct SpawnArea {
    /*
     * Full-line block comments inside structs should not be parsed as fields.
     */
    // Full-line C++ style comments inside structs should be ignored too.
    Point2d center;
    float radius; // C++ style comments should be ignored too.
} SpawnArea;

CXGN_ARRAY_TYPEDEF(Point2d, Point2dArray)
CXGN_OPTIONAL_TYPEDEF(Point2d, OptionalPoint2d)

/**
 * SceneConfig deliberately mixes comment styles so the parser sees realistic
 * headers, including punctuation such as `name`, "quotes", and apostrophes.
 */
typedef struct SceneConfig {
    const char* name;
    const char* note;
    Vec3 background;
    // Waypoints exercise array aliases after comments.
    Point2dArray waypoints;
    const char* skybox;
    OptionalPoint2d previewPoint;
    const Point2d* pointTarget;
    const SpawnArea* spawnArea;
    int maxObjects;
} SceneConfig;
