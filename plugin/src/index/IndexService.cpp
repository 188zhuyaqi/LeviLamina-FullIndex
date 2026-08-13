#include "index/IndexService.h"

#include "protocol/Protocol.h"

namespace fullindex::index {

nlohmann::json IndexService::execute(
    std::string const& requestId,
    std::string const& action,
    nlohmann::json const&
) {
    using namespace protocol;

    if (action == "system.capabilities") {
        return makeResponse(requestId, action, {
            {"runtime", mRuntime.available()},
            {"storage", mStorage.available()},
            {"features", {
                {"players", true},
                {"playerInventory", false},
                {"containers", false},
                {"drops", false},
                {"entities", false},
                {"storageScan", false},
                {"mutation", false}
            }}
        });
    }

    if (action == "players.list") {
        nlohmann::json rows = nlohmann::json::array();

        if (mRuntime.available()) {
            for (auto const& player : mRuntime.listPlayers()) {
                rows.push_back(playerToJson(player));
            }
        }

        // M2 完成后，这里会把离线 Storage 结果与 Runtime 结果按 XUID/UUID 去重合并，
        // Runtime 对同一玩家拥有更高优先级。
        return makeResponse(requestId, action, {
            {"items", std::move(rows)},
            {"sourcePriority", {"runtime", "storage"}}
        });
    }

    if (action == "drops.list") {
        return makeResponse(requestId, action, {
            {"items", nlohmann::json::array()},
            {"implemented", false}
        });
    }

    if (action == "entities.list") {
        return makeResponse(requestId, action, {
            {"items", nlohmann::json::array()},
            {"implemented", false}
        });
    }

    if (action == "containers.list") {
        return makeResponse(requestId, action, {
            {"items", nlohmann::json::array()},
            {"implemented", false}
        });
    }

    return makeError(requestId, action, "unknown action");
}

} // namespace fullindex::index
