#pragma once

#include <cxgen/Array.hpp>
#include <cxgen/Optional.hpp>

template<typename T>
using Vec = Array<T>;

template<typename T>
using Maybe = Optional<T>;

struct TypeOptionsConfig {
    Vec<int> values;
    Maybe<int> max_items;
};
