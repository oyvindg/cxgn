#include "scene.gen.h"
#include <stdio.h>

int main(void) {
    printf("Scene: %s\n", config.name);
    printf("Waypoints: %zu\n", config.waypoints.count);
    if (config.previewPoint.has_value) {
        printf("Preview point: %.1f, %.1f\n",
               config.previewPoint.value.x,
               config.previewPoint.value.y);
    }
    if (config.pointTarget) {
        printf("Target point: %.1f, %.1f\n",
               config.pointTarget->x,
               config.pointTarget->y);
    } else if (config.spawnArea) {
        printf("Target area: %.1f, %.1f radius %.1f\n",
               config.spawnArea->center.x,
               config.spawnArea->center.y,
               config.spawnArea->radius);
    }
    return 0;
}
