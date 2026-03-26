#pragma once

#include <cxgen/Array.hpp>
#include <cxgen/OneOf.hpp>
#include <string>

struct RuleGroup {
    Array<std::string> must;
    Array<std::string> should;
    Array<std::string> must_not;
};

struct OneOfConfig {
    Array<OneOf<std::string, RuleGroup>> rules;
};
