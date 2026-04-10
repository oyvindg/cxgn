#pragma once

#include <stdbool.h>

typedef struct ServerConfig {
    int port;
    int timeout;
} ServerConfig;

typedef struct NestedConfig {
    const char* name;
    ServerConfig server;
    bool enabled;
} NestedConfig;
