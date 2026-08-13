#include "ws/GatewayClient.h"

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

namespace fullindex::ws {

GatewayClient::GatewayClient(config::Config config, index::IndexService& indexService)
: mConfig(std::move(config)),
  mIndexService(indexService),
  mSocket(std::make_unique<ix::WebSocket>()) {}

GatewayClient::~GatewayClient() {
    stop();
}

void GatewayClient::start() {
    mSocket->setUrl(mConfig.gatewayUrl);
    mSocket->setExtraHeaders({
        {"Authorization", "Bearer " + mConfig.pluginToken},
        {"X-FullIndex-Server-Id", mConfig.serverId},
    });

    mSocket->setOnMessageCallback([this](ix::WebSocketMessagePtr const& message) {
        switch (message->type) {
        case ix::WebSocketMessageType::Open: {
            mConnected.store(true);
            nlohmann::json hello = {
                {"type", "hello"},
                {"role", "plugin"},
                {"serverId", mConfig.serverId},
                {"protocolVersion", 1},
                {"pluginVersion", "0.1.0-dev"}
            };
            sendJson(hello.dump());
            break;
        }
        case ix::WebSocketMessageType::Close:
        case ix::WebSocketMessageType::Error:
            mConnected.store(false);
            break;
        case ix::WebSocketMessageType::Message:
            if (!message->binary) {
                handleText(message->str);
            }
            break;
        default:
            break;
        }
    });

    mSocket->start();
}

void GatewayClient::stop() {
    if (mSocket) {
        mSocket->stop();
    }
    mConnected.store(false);
}

void GatewayClient::handleText(std::string const& text) {
    try {
        auto request = nlohmann::json::parse(text);
        if (request.value("type", "") != "request") {
            return;
        }

        auto const requestId = request.value("requestId", "");
        auto const action = request.value("action", "");
        auto const params = request.value("params", nlohmann::json::object());

        auto response = mIndexService.execute(requestId, action, params);
        sendJson(response.dump());
    } catch (std::exception const& e) {
        nlohmann::json response = {
            {"type", "protocol.error"},
            {"error", e.what()}
        };
        sendJson(response.dump());
    } catch (...) {
        nlohmann::json response = {
            {"type", "protocol.error"},
            {"error", "unknown C++ exception"}
        };
        sendJson(response.dump());
    }
}

void GatewayClient::sendJson(std::string const& jsonText) {
    if (mSocket) {
        mSocket->sendText(jsonText);
    }
}

} // namespace fullindex::ws
