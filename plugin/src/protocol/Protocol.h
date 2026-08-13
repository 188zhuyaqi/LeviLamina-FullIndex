#pragma once

#include "model/Records.h"

#include <nlohmann/json.hpp>

namespace fullindex::protocol {

inline nlohmann::json itemToJson(model::ItemRecord const& item) {
    nlohmann::json children = nlohmann::json::array();
    for (auto const& child : item.children) {
        children.push_back(itemToJson(child));
    }

    return {
        {"slot", item.slot},
        {"slotName", item.slotName},
        {"id", item.id},
        {"displayName", item.displayName},
        {"count", item.count},
        {"aux", item.aux},
        {"damage", item.damage},
        {"enchanted", item.enchanted},
        {"hasContainerData", item.hasContainerData},
        {"customName", item.customName},
        {"lore", item.lore},
        {"children", std::move(children)},
    };
}

inline nlohmann::json itemsToJson(std::vector<model::ItemRecord> const& items) {
    nlohmann::json result = nlohmann::json::array();
    for (auto const& item : items) {
        result.push_back(itemToJson(item));
    }
    return result;
}

inline nlohmann::json playerToJson(model::PlayerRecord const& p) {
    nlohmann::json offhand = nullptr;
    if (p.offhand) {
        offhand = itemToJson(*p.offhand);
    }

    return {
        {"source", p.source},
        {"name", p.name},
        {"realName", p.realName},
        {"xuid", p.xuid},
        {"uuid", p.uuid},
        {"storageIds", p.storageIds},
        {"online", p.online},
        {"dimension", p.dimension},
        {"position", {{"x", p.position.x}, {"y", p.position.y}, {"z", p.position.z}}},
        {"selectedSlot", p.selectedSlot},
        {"inventory", itemsToJson(p.inventory)},
        {"armor", itemsToJson(p.armor)},
        {"offhand", std::move(offhand)},
        {"enderChest", itemsToJson(p.enderChest)},
    };
}

inline nlohmann::json entityToJson(model::EntityRecord const& e) {
    return {
        {"source", e.source},
        {"typeName", e.typeName},
        {"customName", e.customName},
        {"category", e.category},
        {"dimension", e.dimension},
        {"position", {{"x", e.position.x}, {"y", e.position.y}, {"z", e.position.z}}},
        {"chunkX", e.chunkX},
        {"chunkZ", e.chunkZ},
    };
}

inline nlohmann::json dropToJson(model::DropRecord const& d) {
    return {
        {"source", d.source},
        {"itemId", d.itemId},
        {"displayName", d.displayName},
        {"stackCount", d.stackCount},
        {"dimension", d.dimension},
        {"position", {{"x", d.position.x}, {"y", d.position.y}, {"z", d.position.z}}},
        {"chunkX", d.chunkX},
        {"chunkZ", d.chunkZ},
    };
}

inline nlohmann::json containerToJson(model::ContainerRecord const& c) {
    std::int64_t itemCount = 0;
    for (auto const& item : c.items) {
        itemCount += item.count;
    }

    return {
        {"source", c.source},
        {"kind", c.kind},
        {"dimension", c.dimension},
        {"position", {{"x", c.position.x}, {"y", c.position.y}, {"z", c.position.z}}},
        {"chunkX", c.chunkX},
        {"chunkZ", c.chunkZ},
        {"occupiedSlots", c.items.size()},
        {"itemCount", itemCount},
        {"items", itemsToJson(c.items)},
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
