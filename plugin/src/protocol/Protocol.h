#pragma once

#include "model/Records.h"

#include <nlohmann/json.hpp>

namespace fullindex::protocol {

inline nlohmann::json playerToJson(model::PlayerRecord const& p) {
    return {
        {"name", p.name},
        {"xuid", p.xuid},
        {"uuid", p.uuid},
        {"online", p.online},
        {"dimension", p.dimension},
        {"position", {{"x", p.position.x}, {"y", p.position.y}, {"z", p.position.z}}},
    };
}

inline nlohmann::json entityToJson(model::EntityRecord const& e) {
    return {
        {"typeName", e.typeName},
        {"category", e.category},
        {"dimension", e.dimension},
        {"position", {{"x", e.position.x}, {"y", e.position.y}, {"z", e.position.z}}},
        {"chunkX", e.chunkX},
        {"chunkZ", e.chunkZ},
    };
}

inline nlohmann::json dropToJson(model::DropRecord const& d) {
    return {
        {"itemId", d.itemId},
        {"displayName", d.displayName},
        {"stackCount", d.stackCount},
        {"dimension", d.dimension},
        {"position", {{"x", d.position.x}, {"y", d.position.y}, {"z", d.position.z}}},
        {"chunkX", d.chunkX},
        {"chunkZ", d.chunkZ},
    };
}

inline nlohmann::json makeResponse(
    std::string const& requestId,
    std::string const& action,
    nlohmann::json data
) {
    return {
        {"type", "response"},
        {"requestId", requestId},
        {"action", action},
        {"ok", true},
        {"data", std::move(data)},
    };
}

inline nlohmann::json makeError(
    std::string const& requestId,
    std::string const& action,
    std::string const& error
) {
    return {
        {"type", "response"},
        {"requestId", requestId},
        {"action", action},
        {"ok", false},
        {"error", error},
    };
}

} // namespace fullindex::protocol
