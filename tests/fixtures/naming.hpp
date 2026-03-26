#pragma once

#include <string>
struct NamingConfig {
    int maxRetryCount;
    std::string serverHost;
    bool isEnabled;
    double connectionTimeout;
};
