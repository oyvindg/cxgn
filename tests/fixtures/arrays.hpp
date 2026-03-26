#pragma once
#include <cxgen/Array.hpp>
#include <string>

struct ItemConfig {
    int id;
    std::string label;
};

struct ArrayConfig {
    Array<int> values;
    Array<ItemConfig> items;
    std::string name;
};
