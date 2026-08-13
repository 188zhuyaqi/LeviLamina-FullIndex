#pragma once

#include <cstdint>
#include <string>

namespace fullindex::config {

struct Config {
    std::string gatewayUrl{"ws://127.0.0.1:30110/ws/plugin"};
    std::string pluginToken{"CHANGE_ME"};
    std::string serverId{"default"};
    std::uint32_t heartbeatSeconds{15};
    bool enableRuntimeProvider{true};
    bool enableStorageProvider{true};
};

bool loadOrCreate(Config& config);

} // namespace fullindex::config
