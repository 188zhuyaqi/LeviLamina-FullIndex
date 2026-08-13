#include "ws/GatewayClient.h"

#include <ll/api/thread/ServerThreadExecutor.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace fullindex::ws {

GatewayClient::GatewayClient(config::Config config, index::IndexService& indexService)
: mConfig(std::move(config)),
  mIndexService(indexService),
  mSocket(std::make_shared<ix::WebSocket>()) {}

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
                {"protocolVersion", 3},
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

        auto socket = mSocket;
        if (action == "index.start") {
            startIndexJob(socket, requestId, params);
            return;
        }
        if (action == "index.cancel") {
            cancelIndexJob(requestId, params);
            return;
        }
        if (action == "live.query.start") {
            startLiveQuery(socket, requestId, params);
            return;
        }
        if (action == "live.query.cancel") {
            cancelLiveQuery(requestId, params);
            return;
        }

        auto* service = &mIndexService;
        ll::thread::ServerThreadExecutor::getDefault().execute(
            [socket = std::move(socket), service, requestId, action, params] {
                auto send = [&socket](nlohmann::json const& response) {
                    if (socket) {
                        socket->sendText(response.dump());
                    }
                };
                try {
                    send(service->execute(requestId, action, params));
                } catch (std::exception const& e) {
                    send(nlohmann::json({
                        {"type", "protocol.error"},
                        {"requestId", requestId},
                        {"action", action},
                        {"error", e.what()},
                    }));
                } catch (...) {
                    send(nlohmann::json({
                        {"type", "protocol.error"},
                        {"requestId", requestId},
                        {"action", action},
                        {"error", "unknown C++ exception"},
                    }));
                }
            }
        );
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

void GatewayClient::startIndexJob(
    std::shared_ptr<ix::WebSocket> const& socket,
    std::string const& requestId,
    nlohmann::json const& params
) {
    auto const jobId = params.value("jobId", "");
    if (jobId.empty()) {
        sendJson(nlohmann::json({
            {"type", "response"},
            {"requestId", requestId},
            {"action", "index.start"},
            {"ok", false},
            {"error", "jobId is required"},
        }).dump());
        return;
    }

    {
        std::scoped_lock lock(mJobMutex);
        if (!mActiveJobId.empty()) {
            sendJson(nlohmann::json({
                {"type", "response"},
                {"requestId", requestId},
                {"action", "index.start"},
                {"ok", false},
                {"error", "an index job is already running"},
                {"data", {{"jobId", mActiveJobId}}},
            }).dump());
            return;
        }
        mActiveJobId = jobId;
        mCancelRequested.store(false);
    }

    auto const requestedBatchSize = params.value("batchSize", 100);
    auto const batchSize = static_cast<std::size_t>(std::clamp(requestedBatchSize, 10, 500));
    sendJson(nlohmann::json({
        {"type", "response"},
        {"requestId", requestId},
        {"action", "index.start"},
        {"ok", true},
        {"data", {{"jobId", jobId}, {"accepted", true}, {"batchSize", batchSize}}},
    }).dump());

    ll::thread::ServerThreadExecutor::getDefault().execute(
        [this, socket, jobId, batchSize] { runIndexJob(socket, jobId, batchSize); }
    );
}

void GatewayClient::cancelIndexJob(std::string const& requestId, nlohmann::json const& params) {
    auto const requestedJobId = params.value("jobId", "");
    std::string activeJobId;
    {
        std::scoped_lock lock(mJobMutex);
        activeJobId = mActiveJobId;
        if (!activeJobId.empty() && (requestedJobId.empty() || requestedJobId == activeJobId)) {
            mCancelRequested.store(true);
        }
    }

    auto const accepted = !activeJobId.empty()
        && (requestedJobId.empty() || requestedJobId == activeJobId);
    sendJson(nlohmann::json({
        {"type", "response"},
        {"requestId", requestId},
        {"action", "index.cancel"},
        {"ok", accepted},
        {"data", {{"jobId", activeJobId}, {"cancelRequested", accepted}}},
        {"error", accepted ? "" : "index job is not running"},
    }).dump());
}

void GatewayClient::runIndexJob(
    std::shared_ptr<ix::WebSocket> socket,
    std::string jobId,
    std::size_t batchSize
) {
    using Clock = std::chrono::steady_clock;
    auto const startedAt = Clock::now();
    constexpr std::array<std::pair<char const*, char const*>, 4> datasets{{
        {"players", "players.list"},
        {"containers", "containers.list"},
        {"drops", "drops.list"},
        {"entities", "entities.list"},
    }};
    nlohmann::json counts = nlohmann::json::object();

    auto send = [&socket](nlohmann::json const& message) {
        if (socket) {
            socket->sendText(message.dump());
        }
    };
    auto cancelled = [&] { return mCancelRequested.load(); };
    auto sendCancelled = [&] {
        send({
            {"type", "index.cancelled"},
            {"jobId", jobId},
            {"counts", counts},
        });
        finishIndexJob(jobId);
    };

    try {
        for (std::size_t datasetIndex = 0; datasetIndex < datasets.size(); ++datasetIndex) {
            if (cancelled()) {
                sendCancelled();
                return;
            }

            auto const& [kind, action] = datasets[datasetIndex];
            send({
                {"type", "index.progress"},
                {"jobId", jobId},
                {"phase", "scanning"},
                {"kind", kind},
                {"completedKinds", datasetIndex},
                {"totalKinds", datasets.size()},
                {"percent", datasetIndex * 100 / datasets.size()},
            });

            auto response = mIndexService.execute(jobId, action, {
                {"all", true},
                {"scope", "runtime_and_storage"},
            }, cancelled);
            if (!response.value("ok", false)) {
                throw std::runtime_error(response.value("error", "index scan failed"));
            }
            auto items = std::move(response["data"]["items"]);
            counts[kind] = items.size();
            auto const batchCount = items.empty() ? std::size_t{0}
                : (items.size() + batchSize - 1) / batchSize;

            for (std::size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
                if (cancelled()) {
                    sendCancelled();
                    return;
                }
                auto const begin = batchIndex * batchSize;
                auto const end = std::min(begin + batchSize, items.size());
                nlohmann::json batch = nlohmann::json::array();
                for (auto itemIndex = begin; itemIndex < end; ++itemIndex) {
                    batch.push_back(std::move(items[itemIndex]));
                }
                send({
                    {"type", "index.batch"},
                    {"jobId", jobId},
                    {"kind", kind},
                    {"batchIndex", batchIndex},
                    {"batchCount", batchCount},
                    {"replace", batchIndex == 0},
                    {"items", std::move(batch)},
                });
            }

            send({
                {"type", "index.progress"},
                {"jobId", jobId},
                {"phase", "streamed"},
                {"kind", kind},
                {"completedKinds", datasetIndex + 1},
                {"totalKinds", datasets.size()},
                {"percent", (datasetIndex + 1) * 100 / datasets.size()},
                {"count", counts[kind]},
            });
        }

        auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - startedAt
        ).count();
        send({
            {"type", "index.complete"},
            {"jobId", jobId},
            {"counts", counts},
            {"durationMs", duration},
        });
    } catch (std::exception const& error) {
        send({
            {"type", "index.failed"},
            {"jobId", jobId},
            {"error", error.what()},
            {"counts", counts},
        });
    } catch (...) {
        send({
            {"type", "index.failed"},
            {"jobId", jobId},
            {"error", "unknown index job error"},
            {"counts", counts},
        });
    }
    finishIndexJob(jobId);
}

void GatewayClient::finishIndexJob(std::string const& jobId) {
    std::scoped_lock lock(mJobMutex);
    if (mActiveJobId == jobId) {
        mActiveJobId.clear();
        mCancelRequested.store(false);
    }
}

void GatewayClient::startLiveQuery(
    std::shared_ptr<ix::WebSocket> const& socket,
    std::string const& requestId,
    nlohmann::json const& params
) {
    auto const jobId = params.value("jobId", "");
    auto const kind = params.value("kind", "");
    constexpr std::array<char const*, 5> supportedKinds{
        "players", "containers", "drops", "entities", "items"
    };
    auto const supported = std::find(supportedKinds.begin(), supportedKinds.end(), kind)
        != supportedKinds.end();
    if (jobId.empty() || !supported) {
        sendJson(nlohmann::json({
            {"type", "response"}, {"requestId", requestId}, {"action", "live.query.start"},
            {"ok", false}, {"error", "jobId or kind is invalid"},
        }).dump());
        return;
    }

    {
        std::scoped_lock lock(mJobMutex);
        if (!mActiveJobId.empty()) {
            sendJson(nlohmann::json({
                {"type", "response"}, {"requestId", requestId}, {"action", "live.query.start"},
                {"ok", false}, {"error", "another scan job is already running"},
                {"data", {{"jobId", mActiveJobId}}},
            }).dump());
            return;
        }
        mActiveJobId = jobId;
        mCancelRequested.store(false);
    }

    auto const requestedBatchSize = params.value("batchSize", 200);
    auto const batchSize = static_cast<std::size_t>(std::clamp(requestedBatchSize, 25, 500));
    auto const filters = params.value("filters", nlohmann::json::object());
    sendJson(nlohmann::json({
        {"type", "response"}, {"requestId", requestId}, {"action", "live.query.start"},
        {"ok", true},
        {"data", {{"jobId", jobId}, {"accepted", true}, {"kind", kind}, {"batchSize", batchSize}}},
    }).dump());

    ll::thread::ServerThreadExecutor::getDefault().execute(
        [this, socket, jobId, kind, filters, batchSize] {
            runLiveQuery(socket, jobId, kind, filters, batchSize);
        }
    );
}

void GatewayClient::cancelLiveQuery(std::string const& requestId, nlohmann::json const& params) {
    auto const requestedJobId = params.value("jobId", "");
    std::string activeJobId;
    {
        std::scoped_lock lock(mJobMutex);
        activeJobId = mActiveJobId;
        if (!activeJobId.empty() && requestedJobId == activeJobId) {
            mCancelRequested.store(true);
        }
    }
    auto const accepted = !activeJobId.empty() && requestedJobId == activeJobId;
    sendJson(nlohmann::json({
        {"type", "response"}, {"requestId", requestId}, {"action", "live.query.cancel"},
        {"ok", accepted},
        {"data", {{"jobId", activeJobId}, {"cancelRequested", accepted}}},
        {"error", accepted ? "" : "live query is not running"},
    }).dump());
}

void GatewayClient::runLiveQuery(
    std::shared_ptr<ix::WebSocket> socket,
    std::string jobId,
    std::string kind,
    nlohmann::json filters,
    std::size_t batchSize
) {
    using Clock = std::chrono::steady_clock;
    auto const startedAt = Clock::now();
    auto send = [&socket](nlohmann::json const& message) {
        if (socket) socket->sendText(message.dump());
    };
    auto cancelled = [&] { return mCancelRequested.load(); };

    try {
        std::size_t total = 0;
        std::size_t nextBatchIndex = 0;
        auto streamItems = [&](nlohmann::json items, std::size_t phasePercent) {
            auto const partCount = items.size();
            auto const partBatches = partCount == 0 ? std::size_t{0}
                : (partCount + batchSize - 1) / batchSize;
            for (std::size_t partBatch = 0; partBatch < partBatches; ++partBatch) {
                if (cancelled()) return;
                auto const begin = partBatch * batchSize;
                auto const end = std::min(begin + batchSize, partCount);
                nlohmann::json batch = nlohmann::json::array();
                for (auto index = begin; index < end; ++index) batch.push_back(std::move(items[index]));
                total += end - begin;
                send({
                    {"type", "live.query.batch"}, {"jobId", jobId}, {"kind", kind},
                    {"batchIndex", nextBatchIndex++}, {"received", total}, {"matched", total},
                    {"percent", phasePercent}, {"items", std::move(batch)},
                });
            }
        };

        if (kind == "items") {
            auto const requestedSource = filters.value("sourceType", "");
            constexpr std::array<std::tuple<char const*, char const*, int>, 3> sources{{
                {"player", "scanning_players", 15},
                {"container", "scanning_containers", 50},
                {"drop", "scanning_drops", 75},
            }};
            for (auto const& [sourceType, phase, percent] : sources) {
                if (!requestedSource.empty() && requestedSource != sourceType) continue;
                if (cancelled()) break;
                send({
                    {"type", "live.query.progress"}, {"jobId", jobId}, {"kind", kind},
                    {"phase", phase}, {"percent", percent}, {"received", total}, {"matched", total},
                });
                auto partFilters = filters;
                partFilters["sourceType"] = sourceType;
                streamItems(mIndexService.liveQuery(kind, partFilters, cancelled), percent);
            }
        } else {
            auto phase = "scanning_" + kind;
            if (kind == "containers") {
                auto const constrainedByPosition = filters.contains("x") && !filters["x"].is_null()
                    && filters.contains("z") && !filters["z"].is_null();
                auto const constrainedByChunk = filters.contains("chunkX") && !filters["chunkX"].is_null()
                    && filters.contains("chunkZ") && !filters["chunkZ"].is_null();
                if (constrainedByPosition) phase = "reading_container_point";
                else if (constrainedByChunk) phase = "reading_container_chunk";
            }
            send({
                {"type", "live.query.progress"}, {"jobId", jobId}, {"kind", kind},
                {"phase", phase}, {"percent", 10}, {"received", 0},
            });
            auto items = mIndexService.liveQuery(kind, filters, cancelled);
            if (!cancelled()) {
                send({
                    {"type", "live.query.progress"}, {"jobId", jobId}, {"kind", kind},
                    {"phase", "streaming"}, {"percent", 75}, {"matched", items.size()},
                    {"received", 0},
                });
                streamItems(std::move(items), 90);
            }
        }
        if (cancelled()) {
            send({{"type", "live.query.cancelled"}, {"jobId", jobId}, {"kind", kind}});
            finishIndexJob(jobId);
            return;
        }

        auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - startedAt
        ).count();
        send({
            {"type", "live.query.complete"}, {"jobId", jobId}, {"kind", kind},
            {"count", total}, {"percent", 100}, {"durationMs", duration},
        });
    } catch (std::exception const& error) {
        send({
            {"type", "live.query.failed"}, {"jobId", jobId}, {"kind", kind},
            {"error", error.what()},
        });
    } catch (...) {
        send({
            {"type", "live.query.failed"}, {"jobId", jobId}, {"kind", kind},
            {"error", "unknown live query error"},
        });
    }
    finishIndexJob(jobId);
}

void GatewayClient::sendJson(std::string const& jsonText) {
    if (mSocket) {
        mSocket->sendText(jsonText);
    }
}

} // namespace fullindex::ws
