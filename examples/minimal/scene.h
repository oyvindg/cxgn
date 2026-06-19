#pragma once

#include <cxgn/macros.h>

/* Minimal schema used by examples/minimal. */
typedef struct Point2d {
    float x; /* horizontal axis */
    float y; /* vertical axis */
} Point2d;

/** Optional target area for the scene. */
typedef struct SpawnArea {
    /*
     * Full-line block comments inside structs are accepted by cxgn's
     * header parser and are ignored during schema extraction.
     */
    // Full-line C++ style comments are ignored during schema extraction.
    Point2d center;
    float radius; // Radius in scene units.
} SpawnArea;

CXGN_ARRAY_TYPEDEF(Point2d, Point2dArray)
CXGN_OPTIONAL_TYPEDEF(Point2d, OptionalPoint2d)

/**
 * SceneConfig mixes arrays, optionals, pointers, and comments.
 * Punctuation inside comments, such as `skybox` or "preview", is ignored.
 */
typedef struct SceneConfig {
    const char* name;
    // Waypoints exercise array aliases after a double-slash comment.
    Point2dArray waypoints;
    const char* skybox;
    OptionalPoint2d previewPoint;
    const Point2d* pointTarget;
    const SpawnArea* spawnArea;
} SceneConfig;
