#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fullindex::model {

struct Vec3Record {
    float x{};
    float y{};
    float z{};
};

struct ItemRecord {
    std::int32_t slot{-1};
    std::string slotName;
    std::string id;
    std::string displayName;
    std::int32_t count{};
    std::int32_t aux{};
    std::int32_t damage{};
    bool enchanted{};
    bool hasContainerData{};
    std::string customName;
    std::vector<std::string> lore;
    std::vector<ItemRecord> children;
};

struct PlayerRecord {
    std::string name;
    std::string xuid;
    std::string uuid;
    bool online{};
    std::string dimension;
    Vec3Record position;
    std::int32_t selectedSlot{-1};
    std::vector<ItemRecord> inventory;
    std::vector<ItemRecord> armor;
    std::optional<ItemRecord> offhand;
    std::vector<ItemRecord> enderChest;
};

struct EntityRecord {
    std::string typeName;
    std::string category;
    std::string dimension;
    Vec3Record position;
    std::int32_t chunkX{};
    std::int32_t chunkZ{};
};

struct DropRecord {
    std::string itemId;
    std::string displayName;
    std::int32_t stackCount{};
    std::string dimension;
    Vec3Record position;
    std::int32_t chunkX{};
    std::int32_t chunkZ{};
};

struct ContainerRecord {
    std::string kind;
    std::string dimension;
    Vec3Record position;
    std::int32_t chunkX{};
    std::int32_t chunkZ{};
    std::vector<ItemRecord> items;
};

} // namespace fullindex::model
