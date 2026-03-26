#pragma once

/**
 * @brief Simple test struct for cxgen.
 */
#include <string>
struct SimpleConfig {
    int timeout;
    std::string name;
    bool enabled = true;
};
