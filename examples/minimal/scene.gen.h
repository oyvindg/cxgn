// GENERATED FILE - DO NOT EDIT
// Source YAML: examples/minimal/scene.yaml
// Source Header: examples/minimal/scene.h
#ifndef CXGN_EXAMPLES_MINIMAL_SCENE_GEN_H
#define CXGN_EXAMPLES_MINIMAL_SCENE_GEN_H

#include <stddef.h>
#include <stdbool.h>
#include <cxgn/macros.h>
#include "scene.h"

static const Point2d _backing_SceneConfig_waypoints_0_data[] = {{
        .x = 1.0,
        .y = 2.0
    }, {
        .x = 3.0,
        .y = 4.0
    }};
static const SpawnArea _backing_SceneConfig_spawnArea_1 = {
        .center = {
            .x = 8.0,
            .y = 6.0
        },
        .radius = 3.5
    };

static const SceneConfig config = {
    .name = "demo",
    .waypoints = {.data = _backing_SceneConfig_waypoints_0_data, .count = 2},
    .skybox = 0,
    .previewPoint = {.has_value = false},
    .pointTarget = 0,
    .spawnArea = &_backing_SceneConfig_spawnArea_1
};

#endif /* CXGN_EXAMPLES_MINIMAL_SCENE_GEN_H */
