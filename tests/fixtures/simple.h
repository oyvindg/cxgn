#pragma once

#include <stdbool.h>

typedef struct SimpleConfig {
    int timeout;
    const char* name;
    bool enabled;
} SimpleConfig;
