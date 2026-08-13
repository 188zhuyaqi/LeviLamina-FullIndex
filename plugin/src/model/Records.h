#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fullindex::model {

struct Vec3Record {
    float x{};
    float y{};
    float z{};
};

struct ItemRecord {
    std::string id;
    std::string displayName;
    std::int32_t count{};
    std::int32_t aux{};
    std::string customName;
    std::vector<ItemRecord> children;
};

struct PlayerRecord {
    std::string name;
    std::string xuid;
    std::string uuid;
    bool online{};
    std::string dimension;
    Vec3Record position;
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
