#pragma once

#include <cxgn/Array.hpp>
#include <variant>
#include <string>

struct RuleGroup {
    Array<std::string> must;
    Array<std::string> should;
    Array<std::string> must_not;
};

struct VariantConfig {
    Array<std::variant<std::string, RuleGroup>> rules;
};
