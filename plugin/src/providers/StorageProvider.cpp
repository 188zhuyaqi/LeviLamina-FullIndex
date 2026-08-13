#include "providers/StorageProvider.h"

#include "serialization/ItemSerializer.h"
#include "util/ChunkMath.h"

#include <ll/api/service/Bedrock.h>
#include <ll/api/service/PlayerInfo.h>
#include <mc/deps/nbt/CompoundTag.h>
#include <mc/deps/nbt/ListTag.h>
#include <mc/deps/nbt/NbtIo.h>
#include <mc/util/StringByteInput.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/chunk/LevelChunkTag.h>
#include <mc/world/level/storage/DBStorage.h>
#include <mc/world/level/storage/LevelStorage.h>
#include <mc/world/level/storage/PlayerDataSystem.h>
#include <mc/world/level/storage/db_helpers/Category.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace fullindex::providers {
namespace {

std::string dimensionName(std::int32_t dimension) {
    switch (dimension) {
    case 0:
        return "overworld";
    case 1:
        return "nether";
    case 2:
        return "the_end";
    default:
        return "dimension:" + std::to_string(dimension);
    }
}

std::optional<std::int32_t> dimensionId(std::string const& dimension) {
    if (dimension == "overworld") return 0;
    if (dimension == "nether") return 1;
    if (dimension == "the_end") return 2;
    constexpr std::string_view prefix{"dimension:"};
    if (!dimension.starts_with(prefix)) return std::nullopt;
    try {
        return std::stoi(dimension.substr(prefix.size()));
    } catch (...) {
        return std::nullopt;
    }
}

std::string chunkIdentity(std::string const& dimension, std::int32_t x, std::int32_t z) {
    return dimension + ":" + std::to_string(x) + ":" + std::to_string(z);
}

std::string blockEntityKey(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t dimension) {
    std::string key;
    key.reserve(dimension == 0 ? 9 : 13);
    key.append(reinterpret_cast<char const*>(&chunkX), sizeof(chunkX));
    key.append(reinterpret_cast<char const*>(&chunkZ), sizeof(chunkZ));
    if (dimension != 0) key.append(reinterpret_cast<char const*>(&dimension), sizeof(dimension));
    key.push_back(static_cast<char>(LevelChunkTag::BlockEntity));
    return key;
}

CompoundTagVariant const* find(CompoundTag const& tag, std::string_view name) {
    auto const iterator = tag.mTags.find(name);
    return iterator == tag.mTags.end() ? nullptr : &iterator->second;
}

std::string stringValue(CompoundTag const& tag, std::initializer_list<std::string_view> names) {
    for (auto const name : names) {
        auto const* value = find(tag, name);
        if (value && value->is_string()) {
            return static_cast<std::string const&>(*value);
        }
    }
    return {};
}

std::int32_t intValue(
    CompoundTag const& tag,
    std::initializer_list<std::string_view> names,
    std::int32_t fallback = 0
) {
    for (auto const name : names) {
        auto const* value = find(tag, name);
        if (value && value->is_number_integer()) {
            return static_cast<std::int32_t>(*value);
        }
    }
    return fallback;
}

model::Vec3Record positionValue(CompoundTag const& tag) {
    auto const* value = find(tag, "Pos");
    if (!value || !value->hold<ListTag>()) {
        return {};
    }
    auto const& list = value->get<ListTag>();
    if (list.size() < 3 || !list[0] || !list[1] || !list[2]) {
        return {};
    }
    return {
        static_cast<float>(list[0]),
        static_cast<float>(list[1]),
        static_cast<float>(list[2]),
    };
}

std::vector<model::ItemRecord> itemList(CompoundTag const& owner, std::string_view name, std::string slotName = {}) {
    std::vector<model::ItemRecord> result;
    auto const* value = find(owner, name);
    if (!value || !value->hold<ListTag>()) {
        return result;
    }

    auto const& list = value->get<ListTag>();
    result.reserve(list.size());
    for (std::size_t index = 0; index < list.size(); ++index) {
        auto const& entry = list[index];
        if (!entry || !entry.hold<CompoundTag>()) {
            continue;
        }
        auto const& itemTag = entry.get<CompoundTag>();
        auto stack = ItemStack::fromTag(itemTag);
        if (stack.isNull()) {
            continue;
        }
        auto const slot = intValue(itemTag, {"Slot"}, static_cast<std::int32_t>(index));
        result.emplace_back(serialization::serializeItem(stack, slot, slotName));
    }
    return result;
}

std::vector<model::ItemRecord> firstItemList(
    CompoundTag const& owner,
    std::initializer_list<std::string_view> names,
    std::string slotName = {}
) {
    for (auto const name : names) {
        if (find(owner, name)) {
            return itemList(owner, name, std::move(slotName));
        }
    }
    return {};
}

bool decimalString(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return character >= '0' && character <= '9';
    });
}

std::string normalizedIdentifier(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string canonicalStorageId(LevelStorage& storage, std::string const& storageId) {
    if (storageId.starts_with("server_")) {
        return normalizedIdentifier(storageId);
    }
    auto key = PlayerDataSystem::serverKey(storage, storageId);
    if (key.starts_with("player_")) {
        key.erase(0, 7);
    }
    return normalizedIdentifier(key.empty() ? storageId : key);
}

bool uuidString(std::string_view value) {
    if (value.size() != 36) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (value[index] != '-') {
                return false;
            }
            continue;
        }
        auto const character = static_cast<unsigned char>(value[index]);
        if (!std::isxdigit(character)) {
            return false;
        }
    }
    return true;
}

void appendUnique(std::vector<std::string>& values, std::string_view value) {
    if (value.empty() || std::find(values.begin(), values.end(), value) != values.end()) {
        return;
    }
    values.emplace_back(value);
}

void readNbtSequence(std::string_view buffer, std::function<void(CompoundTag const&)> const& callback) {
    StringByteInput input(buffer);
    while (input.numBytesLeft() > 0) {
        auto const before = input.mIdx;
        auto result = NbtIo::read(input);
        if (!result || !*result) {
            break;
        }
        callback(**result);
        if (input.mIdx <= before) {
            break;
        }
    }
}

std::optional<std::tuple<std::int32_t, std::int32_t, std::int32_t>> blockEntityChunkKey(std::string_view key) {
    constexpr auto tag = static_cast<char>(LevelChunkTag::BlockEntity);
    std::int32_t x{};
    std::int32_t z{};
    std::int32_t dimension{};

    if (key.size() == 9 && key[8] == tag) {
        std::memcpy(&x, key.data(), sizeof(x));
        std::memcpy(&z, key.data() + 4, sizeof(z));
        return std::tuple{x, z, 0};
    }
    if (key.size() == 13 && key[12] == tag) {
        std::memcpy(&x, key.data(), sizeof(x));
        std::memcpy(&z, key.data() + 4, sizeof(z));
        std::memcpy(&dimension, key.data() + 8, sizeof(dimension));
        return std::tuple{x, z, dimension};
    }
    return std::nullopt;
}

bool specialEntity(std::string_view typeName) {
    constexpr std::array<std::string_view, 12> markers{
        "armor_stand",
        "boat",
        "minecart",
        "item_frame",
        "painting",
        "tnt",
        "falling_block",
        "xp_orb",
        "projectile",
        "arrow",
        "fireball",
        "lightning_bolt",
    };
    return std::any_of(markers.begin(), markers.end(), [&](std::string_view marker) {
        return typeName.find(marker) != std::string_view::npos;
    });
}

} // namespace

bool StorageProvider::available() const {
    return ll::service::getDBStorage() != nullptr;
}

std::vector<model::PlayerRecord> StorageProvider::listPlayers(CancelCheck const& shouldCancel) {
    std::vector<model::PlayerRecord> result;
    std::unordered_map<std::string, std::size_t> recordByServerId;
    auto level = ll::service::getLevel();
    if (!level) {
        return result;
    }

    auto& storage = level->getLevelStorage();
    for (auto const& storageId : storage.loadAllPlayerIDs(false)) {
        if (shouldCancel && shouldCancel()) {
            break;
        }
        auto const serverId = canonicalStorageId(storage, storageId);
        auto const existing = recordByServerId.find(serverId);
        if (existing != recordByServerId.end()) {
            appendUnique(result[existing->second].storageIds, storageId);
            continue;
        }

        auto tag = PlayerDataSystem::loadPlayerDataFromTag(storage, storageId);
        if (!tag) {
            continue;
        }

        model::PlayerRecord record;
        record.source = "storage";
        record.online = false;
        record.realName = stringValue(*tag, {"LastKnownPlayerName", "PlayerName", "NameTag"});
        record.xuid = stringValue(*tag, {"XUID", "PlatformOnlineId"});
        record.uuid = stringValue(*tag, {"UUID", "SelfSignedId", "ClientId"});
        appendUnique(record.storageIds, storageId);
        record.dimension = dimensionName(intValue(*tag, {"DimensionId", "Dimension"}));
        record.position = positionValue(*tag);
        record.selectedSlot = intValue(*tag, {"SelectedInventorySlot", "SelectedSlot"}, -1);
        record.inventory = firstItemList(*tag, {"Inventory", "inventory"});
        for (auto& item : record.inventory) {
            item.slotName = item.slot >= 0 && item.slot <= 8
                ? (item.slot == record.selectedSlot ? "hotbar:selected" : "hotbar")
                : "inventory";
        }

        record.armor = firstItemList(*tag, {"Armor", "ArmorItems"}, "armor");
        auto offhand = firstItemList(*tag, {"Offhand", "OffhandItems"}, "offhand");
        if (!offhand.empty()) {
            record.offhand = std::move(offhand.front());
        }
        record.enderChest = firstItemList(*tag, {"EnderChestInventory", "EnderChest"}, "ender_chest");
        recordByServerId.emplace(serverId, result.size());
        result.emplace_back(std::move(record));
    }


    auto& playerInfo = ll::service::PlayerInfo::getInstance();
    for (auto& record : result) {
        std::sort(record.storageIds.begin(), record.storageIds.end());
        if (record.xuid.empty()) {
            auto const xuid = std::find_if(record.storageIds.begin(), record.storageIds.end(), decimalString);
            if (xuid != record.storageIds.end()) {
                record.xuid = *xuid;
            }
        }
        if (record.uuid.empty()) {
            auto const uuid = std::find_if(record.storageIds.begin(), record.storageIds.end(), [](auto const& id) {
                return !id.starts_with("server_") && uuidString(id);
            });
            if (uuid != record.storageIds.end()) {
                record.uuid = *uuid;
            }
        }

        auto applyKnownPlayer = [&](ll::service::PlayerInfo::PlayerInfoEntry const& known) {
            record.realName = known.name;
            record.xuid = known.xuid;
            record.uuid = known.uuid.asString();
        };
        auto resolveUuid = [&](std::string_view value) {
            if (!uuidString(value)) {
                return false;
            }
            auto known = playerInfo.fromUuid(mce::UUID(value));
            if (!known) {
                return false;
            }
            applyKnownPlayer(*known);
            return true;
        };
        auto resolveXuid = [&](std::string_view value) {
            if (!decimalString(value)) {
                return false;
            }
            auto known = playerInfo.fromXuid(value);
            if (!known) {
                return false;
            }
            applyKnownPlayer(*known);
            return true;
        };

        auto resolved = resolveUuid(record.uuid) || resolveXuid(record.xuid);
        for (auto const& id : record.storageIds) {
            if (resolved) {
                break;
            }
            auto const candidate = id.starts_with("server_") ? std::string_view{id}.substr(7) : std::string_view{id};
            resolved = resolveUuid(candidate) || resolveXuid(candidate);
        }

        // 某些旧存档把标识符写进名称字段；这种值不能冒充真实玩家名称。
        if (decimalString(record.realName) || uuidString(record.realName) || record.realName.starts_with("server_")) {
            record.realName.clear();
        }
        if (!record.realName.empty()) {
            record.name = record.realName;
        } else if (!record.xuid.empty()) {
            record.name = record.xuid;
        } else if (!record.uuid.empty()) {
            record.name = record.uuid;
        } else if (!record.storageIds.empty()) {
            record.name = record.storageIds.front();
        }
    }
    return result;
}

std::vector<model::DropRecord> StorageProvider::listDrops(CancelCheck const& shouldCancel) {
    std::vector<model::DropRecord> result;
    auto storage = ll::service::getDBStorage();
    if (!storage) {
        return result;
    }

    storage->forEachKeyWithPrefix(
        "actorprefix",
        DBHelpers::Category::Actor,
        [&](std::string_view, std::string_view value) {
            if (shouldCancel && shouldCancel()) {
                return;
            }
            readNbtSequence(value, [&](CompoundTag const& tag) {
                if (shouldCancel && shouldCancel()) {
                    return;
                }
                auto const typeName = stringValue(tag, {"identifier", "Identifier", "id"});
                if (typeName != "minecraft:item" && typeName.find("item_actor") == std::string::npos) {
                    return;
                }
                auto const* itemValue = find(tag, "Item");
                if (!itemValue || !itemValue->hold<CompoundTag>()) {
                    return;
                }
                auto stack = ItemStack::fromTag(itemValue->get<CompoundTag>());
                if (stack.isNull()) {
                    return;
                }

                auto const position = positionValue(tag);
                model::DropRecord record;
                record.source = "storage";
                record.itemId = stack.getTypeName();
                record.displayName = stack.getDescriptionName();
                record.stackCount = static_cast<std::int32_t>(stack.mCount);
                record.dimension = dimensionName(intValue(tag, {"DimensionId", "Dimension"}));
                record.position = position;
                record.chunkX = util::blockToChunk(position.x);
                record.chunkZ = util::blockToChunk(position.z);
                result.emplace_back(std::move(record));
            });
        }
    );
    return result;
}

std::vector<model::EntityRecord> StorageProvider::listEntities(CancelCheck const& shouldCancel) {
    std::vector<model::EntityRecord> result;
    auto storage = ll::service::getDBStorage();
    if (!storage) {
        return result;
    }

    storage->forEachKeyWithPrefix(
        "actorprefix",
        DBHelpers::Category::Actor,
        [&](std::string_view, std::string_view value) {
            if (shouldCancel && shouldCancel()) {
                return;
            }
            readNbtSequence(value, [&](CompoundTag const& tag) {
                if (shouldCancel && shouldCancel()) {
                    return;
                }
                auto const typeName = stringValue(tag, {"identifier", "Identifier", "id"});
                if (typeName.empty() || typeName == "minecraft:item" || typeName == "minecraft:player") {
                    return;
                }
                auto const position = positionValue(tag);
                model::EntityRecord record;
                record.source = "storage";
                record.typeName = typeName;
                record.customName = stringValue(tag, {"CustomName", "NameTag"});
                record.category = specialEntity(typeName) ? "SPECIAL_ENTITY" : "NATURAL_MOB";
                record.dimension = dimensionName(intValue(tag, {"DimensionId", "Dimension"}));
                record.position = position;
                record.chunkX = util::blockToChunk(position.x);
                record.chunkZ = util::blockToChunk(position.z);
                result.emplace_back(std::move(record));
            });
        }
    );
    return result;
}

std::vector<model::ContainerRecord> StorageProvider::listContainers(CancelCheck const& shouldCancel) {
    return listContainers(ContainerQuery{}, {}, shouldCancel);
}

std::vector<model::ContainerRecord> StorageProvider::listContainers(
    ContainerQuery const& query,
    std::unordered_set<std::string> const& excludedChunks,
    CancelCheck const& shouldCancel
) {
    std::vector<model::ContainerRecord> result;
    auto storage = ll::service::getDBStorage();
    if (!storage) {
        return result;
    }

    auto consume = [&](std::string_view key, std::string_view value) {
            if (shouldCancel && shouldCancel()) {
                return;
            }
            auto const chunkKey = blockEntityChunkKey(key);
            if (!chunkKey) {
                return;
            }
            auto const [chunkX, chunkZ, dimension] = *chunkKey;
            auto const label = dimensionName(dimension);
            if (!query.matchesChunk(label, chunkX, chunkZ)) return;
            if (excludedChunks.contains(chunkIdentity(label, chunkX, chunkZ))) return;
            readNbtSequence(value, [&](CompoundTag const& tag) {
                if (shouldCancel && shouldCancel()) {
                    return;
                }
                auto items = firstItemList(tag, {"Items", "items"});
                auto const kind = stringValue(tag, {"id", "identifier"});
                if (items.empty() && !find(tag, "Items") && !find(tag, "items")) {
                    return;
                }

                model::ContainerRecord record;
                record.source = "storage";
                record.kind = kind.empty() ? "container" : kind;
                record.dimension = dimensionName(dimension);
                record.position = {
                    static_cast<float>(intValue(tag, {"x"})),
                    static_cast<float>(intValue(tag, {"y"})),
                    static_cast<float>(intValue(tag, {"z"})),
                };
                record.chunkX = chunkX;
                record.chunkZ = chunkZ;
                if (!query.matches(record)) return;
                record.items = std::move(items);
                result.emplace_back(std::move(record));
            });
    };

    // 精确坐标和完整区块查询直接读取对应 BlockEntity 键，避免枚举整个世界数据库。
    if (query.hasChunk()) {
        std::array<std::int32_t, 3> const standardDimensions{0, 1, 2};
        if (query.dimension) {
            if (auto id = dimensionId(*query.dimension)) {
                auto const key = blockEntityKey(*query.chunkX, *query.chunkZ, *id);
                std::string value;
                if (storage->loadData(key, value, DBHelpers::Category::Chunk)) consume(key, value);
            }
        } else {
            for (auto const id : standardDimensions) {
                if (shouldCancel && shouldCancel()) break;
                auto const key = blockEntityKey(*query.chunkX, *query.chunkZ, id);
                std::string value;
                if (storage->loadData(key, value, DBHelpers::Category::Chunk)) consume(key, value);
            }
        }
        return result;
    }

    storage->forEachKeyWithPrefix({}, DBHelpers::Category::Chunk, consume);
    return result;
}

} // namespace fullindex::providers
