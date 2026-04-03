#pragma once

/**
 * @brief Simple test struct for cxgn.
 */
#include <string>
struct SimpleConfig {
    int timeout;
    std::string name;
    bool enabled = true;
};
