#pragma once
#include <cxgn/Array.hpp>
#include <cxgn/Optional.hpp>
#include <string>

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Point2d {
    float x;
    float y;
};

struct SceneConfig {
    std::string name;
    Vec3 background;
    Array<Point2d> waypoints;
    Optional<std::string> skybox;
    int maxObjects = 100;
};
