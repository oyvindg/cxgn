#pragma once

#include <stdbool.h>

typedef struct NamingConfig {
    int maxRetryCount;
    const char* serverHost;
    bool isEnabled;
    double connectionTimeout;
} NamingConfig;
