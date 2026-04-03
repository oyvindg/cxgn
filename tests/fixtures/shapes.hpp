#pragma once

#include <variant>

struct Circle {
    float radius;
};

struct Rectangle {
    float width;
    float height;
};

struct ShapeConfig {
    std::variant<Circle, Rectangle> shape;
};
