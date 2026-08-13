#pragma once

#include "config/Config.h"
#include "index/IndexService.h"

#include <atomic>
#include <memory>
#include <mutex>
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
    void startIndexJob(
        std::shared_ptr<ix::WebSocket> const& socket,
        std::string const& requestId,
        nlohmann::json const& params
    );
    void cancelIndexJob(std::string const& requestId, nlohmann::json const& params);
    void runIndexJob(std::shared_ptr<ix::WebSocket> socket, std::string jobId, std::size_t batchSize);
    void startLiveQuery(
        std::shared_ptr<ix::WebSocket> const& socket,
        std::string const& requestId,
        nlohmann::json const& params
    );
    void cancelLiveQuery(std::string const& requestId, nlohmann::json const& params);
    void runLiveQuery(
        std::shared_ptr<ix::WebSocket> socket,
        std::string jobId,
        std::string kind,
        nlohmann::json filters,
        std::size_t batchSize
    );
    void finishIndexJob(std::string const& jobId);

    config::Config mConfig;
    index::IndexService& mIndexService;
    std::shared_ptr<ix::WebSocket> mSocket;
    std::atomic_bool mConnected{false};
    std::atomic_bool mCancelRequested{false};
    std::mutex mJobMutex;
    std::string mActiveJobId;
};

} // namespace fullindex::ws
