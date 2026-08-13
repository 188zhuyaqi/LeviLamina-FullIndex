#include "config/Config.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace fullindex::config {
namespace {
constexpr auto kConfigPath = "plugins/FullIndex/config.json";

nlohmann::json toJson(Config const& config) {
    return {
        {"gatewayUrl", config.gatewayUrl},
        {"pluginToken", config.pluginToken},
        {"serverId", config.serverId},
        {"heartbeatSeconds", config.heartbeatSeconds},
        {"enableRuntimeProvider", config.enableRuntimeProvider},
        {"enableStorageProvider", config.enableStorageProvider},
    };
}
} // namespace

bool loadOrCreate(Config& config) {
    namespace fs = std::filesystem;
    auto const path = fs::path(kConfigPath);

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.good()) {
            return false;
        }
        output << toJson(config).dump(2);
        return true;
    }

    try {
        nlohmann::json json;
        input >> json;
        config.gatewayUrl = json.value("gatewayUrl", config.gatewayUrl);
        config.pluginToken = json.value("pluginToken", config.pluginToken);
        config.serverId = json.value("serverId", config.serverId);
        config.heartbeatSeconds = json.value("heartbeatSeconds", config.heartbeatSeconds);
        config.enableRuntimeProvider = json.value("enableRuntimeProvider", config.enableRuntimeProvider);
        config.enableStorageProvider = json.value("enableStorageProvider", config.enableStorageProvider);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace fullindex::config
