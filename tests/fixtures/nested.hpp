#pragma once

#include <string>
struct ServerConfig {
    int port;
    int timeout = 30;
};

struct NestedConfig {
    std::string name;
    ServerConfig server;
    bool enabled;
};
