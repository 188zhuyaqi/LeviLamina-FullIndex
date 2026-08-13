#pragma once

#include "config/Config.h"
#include "index/IndexService.h"

#include <atomic>
#include <memory>
#include <string>

namespace ix {
class WebSocket;
}

namespace fullindex::ws {

class GatewayClient {
public:
    GatewayClient(config::Config config, index::IndexService& indexService);
    ~GatewayClient();

    GatewayClient(GatewayClient const&) = delete;
    GatewayClient& operator=(GatewayClient const&) = delete;

    void start();
    void stop();
    [[nodiscard]] bool connected() const { return mConnected.load(); }

private:
    void handleText(std::string const& text);
    void sendJson(std::string const& jsonText);

    config::Config mConfig;
    index::IndexService& mIndexService;
    std::unique_ptr<ix::WebSocket> mSocket;
    std::atomic_bool mConnected{false};
};

} // namespace fullindex::ws
