#pragma once
#include <cxgn/Optional.hpp>
#include <string>

struct OptionalConfig {
    std::string name;
    Optional<std::string> description;
    Optional<int> maxItems;
    int minItems = 0;
};
