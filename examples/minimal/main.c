#include "scene.h"
#include "scene.gen.h"
#include <stdio.h>

int main(void) {
    printf("Scene: %s\n", config.name);
    printf("Waypoints: %zu\n", config.waypoints.count);
    return 0;
}
