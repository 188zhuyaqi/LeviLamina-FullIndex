#include "providers/RuntimeProvider.h"

#include "serialization/ItemSerializer.h"
#include "util/ChunkMath.h"

#include <ll/api/service/Bedrock.h>
#include <mc/deps/ecs/WeakEntityRef.h>
#include <mc/entity/components_json_legacy/ContainerComponent.h>
#include <mc/deps/shared_types/legacy/EquipmentSlot.h>
#include <mc/platform/UUID.h>
#include <mc/world/Container.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorCategory.h>
#include <mc/world/actor/item/ItemActor.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/block/actor/BlockActor.h>
#include <mc/world/level/block/actor/BlockActorType.h>
#include <mc/world/level/chunk/ChunkSource.h>
#include <mc/world/level/chunk/LevelChunk.h>
#include <mc/world/level/dimension/Dimension.h>

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace fullindex::providers {
namespace {

std::string dimensionName(DimensionType dimension) {
    switch (dimension.id) {
    case 0:
        return "overworld";
    case 1:
        return "nether";
    case 2:
        return "the_end";
    default:
        return "dimension:" + std::to_string(dimension.id);
    }
}

std::string chunkIdentity(std::string const& dimension, std::int32_t x, std::int32_t z) {
    return dimension + ":" + std::to_string(x) + ":" + std::to_string(z);
}

void appendUnique(std::vector<std::string>& values, std::string const& value) {
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::string_view blockActorKind(BlockActorType type) {
    switch (type) {
    case BlockActorType::Furnace:
        return "furnace";
    case BlockActorType::Chest:
        return "chest";
    case BlockActorType::BrewingStand:
        return "brewing_stand";
    case BlockActorType::Dispenser:
        return "dispenser";
    case BlockActorType::Dropper:
        return "dropper";
    case BlockActorType::Hopper:
        return "hopper";
    case BlockActorType::Cauldron:
        return "cauldron";
    case BlockActorType::EnderChest:
        return "ender_chest";
    case BlockActorType::ShulkerBox:
        return "shulker_box";
    case BlockActorType::BlastFurnace:
        return "blast_furnace";
    case BlockActorType::Smoker:
        return "smoker";
    case BlockActorType::Campfire:
        return "campfire";
    case BlockActorType::BarrelBlock:
        return "barrel";
    case BlockActorType::ChiseledBookshelf:
        return "chiseled_bookshelf";
    case BlockActorType::BrushableBlock:
        return "brushable_block";
    case BlockActorType::DecoratedPot:
        return "decorated_pot";
    case BlockActorType::Crafter:
        return "crafter";
    case BlockActorType::Shelf:
        return "shelf";
    default:
        return "container";
    }
}

template <class Callback>
void forEachLoadedActor(Level& level, Callback&& callback) {
    level.forEachDimension([&](Dimension& dimension) -> bool {
        auto const dimensionId = dimension.getDimensionId();
        auto const* chunkMap = dimension.getChunkSource().getChunkMap();
        if (!chunkMap) {
            return true;
        }

        for (auto const& [chunkPos, weakChunk] : *chunkMap) {
            auto chunk = weakChunk.lock();
            if (!chunk) {
                continue;
            }

            for (auto const& weakActor : chunk->mEntities.get()) {
                auto actor = weakActor.tryUnwrap<Actor>();
                if (actor) {
                    std::invoke(callback, *actor, dimensionId, chunkPos.x, chunkPos.z);
                }
            }
        }
        return true;
    });
}

} // namespace

bool RuntimeProvider::available() const {
    return ll::service::getLevel().has_value();
}

std::vector<model::PlayerRecord> RuntimeProvider::listPlayers(CancelCheck const& shouldCancel) {
    std::vector<model::PlayerRecord> result;

    auto level = ll::service::getLevel();
    if (!level) {
        return result;
    }

    level->forEachPlayer([&](Player& player) -> bool {
        if (shouldCancel && shouldCancel()) {
            return false;
        }
        model::PlayerRecord record;
        record.source = "runtime";
        record.realName = player.getRealName();
        record.name = record.realName;
        record.xuid = player.getXuid();
        record.uuid = player.getUuid().asString();
        appendUnique(record.storageIds, player.mUniqueName.get());
        appendUnique(record.storageIds, player.mServerId.get());
        appendUnique(record.storageIds, player.mSelfSignedId.get());
        appendUnique(record.storageIds, player.mPlatformOfflineId.get());
        record.online = true;
        record.dimension = dimensionName(player.getDimensionId());
        record.selectedSlot = player.getSelectedItemSlot();

        auto const& pos = player.getPosition();
        record.position = {pos.x, pos.y, pos.z};

        record.inventory = serialization::serializeContainer(player.getInventory());
        for (auto& item : record.inventory) {
            if (item.slot >= 0 && item.slot <= 8) {
                item.slotName = item.slot == record.selectedSlot ? "hotbar:selected" : "hotbar";
            } else {
                item.slotName = "inventory";
            }
        }

        using Slot = SharedTypes::Legacy::EquipmentSlot;
        constexpr std::array<std::pair<Slot, char const*>, 4> kArmorSlots{{
            {Slot::Head, "head"},
            {Slot::Torso, "chest"},
            {Slot::Legs, "legs"},
            {Slot::Feet, "feet"},
        }};

        for (auto const& [slot, name] : kArmorSlots) {
            auto const& stack = player.getItemSlot(slot);
            if (!stack.isNull()) {
                record.armor.emplace_back(serialization::serializeItem(stack, -1, name));
            }
        }

        auto const& offhand = player.getItemSlot(Slot::Offhand);
        if (!offhand.isNull()) {
            record.offhand = serialization::serializeItem(offhand, -1, "offhand");
        }

        if (auto enderChest = player.getEnderChestContainer()) {
            record.enderChest = serialization::serializeContainer(*enderChest);
            for (auto& item : record.enderChest) {
                item.slotName = "ender_chest";
            }
        }

        result.emplace_back(std::move(record));
        return true;
    });

    return result;
}

std::vector<model::DropRecord> RuntimeProvider::listDrops(CancelCheck const& shouldCancel) {
    std::vector<model::DropRecord> result;
    auto level = ll::service::getLevel();
    if (!level) {
        return result;
    }

    forEachLoadedActor(*level, [&](Actor& actor, DimensionType dimension, int chunkX, int chunkZ) {
        if (shouldCancel && shouldCancel()) {
            return;
        }
        auto* itemActor = ItemActor::tryGetFromEntity(actor.getEntityContext(), false);
        if (!itemActor) {
            return;
        }

        auto const& stack = itemActor->item();
        if (stack.isNull()) {
            return;
        }

        auto const& pos = actor.getPosition();
        model::DropRecord record;
        record.source = "runtime";
        record.itemId = stack.getTypeName();
        record.displayName = stack.getDescriptionName();
        record.stackCount = static_cast<std::int32_t>(stack.mCount);
        record.dimension = dimensionName(dimension);
        record.position = {pos.x, pos.y, pos.z};
        record.chunkX = chunkX;
        record.chunkZ = chunkZ;
        result.emplace_back(std::move(record));
    });

    return result;
}

std::vector<model::EntityRecord> RuntimeProvider::listEntities(CancelCheck const& shouldCancel) {
    std::vector<model::EntityRecord> result;
    auto level = ll::service::getLevel();
    if (!level) {
        return result;
    }

    forEachLoadedActor(*level, [&](Actor& actor, DimensionType dimension, int chunkX, int chunkZ) {
        if (shouldCancel && shouldCancel()) {
            return;
        }
        if (actor.isPlayer() || ItemActor::tryGetFromEntity(actor.getEntityContext(), false)) {
            return;
        }

        auto const& pos = actor.getPosition();
        model::EntityRecord record;
        record.source = "runtime";
        record.typeName = actor.getTypeName();
        record.customName = actor.getNameTag();
        record.category = actor.hasCategory(ActorCategory::Mob) ? "NATURAL_MOB" : "SPECIAL_ENTITY";
        record.dimension = dimensionName(dimension);
        record.position = {pos.x, pos.y, pos.z};
        record.chunkX = chunkX;
        record.chunkZ = chunkZ;
        result.emplace_back(std::move(record));
    });

    return result;
}

std::vector<model::ContainerRecord> RuntimeProvider::listContainers(CancelCheck const& shouldCancel) {
    return listContainers(ContainerQuery{}, shouldCancel);
}

std::unordered_set<std::string> RuntimeProvider::loadedContainerChunks(ContainerQuery const& query) {
    std::unordered_set<std::string> result;
    auto level = ll::service::getLevel();
    if (!level) return result;

    level->forEachDimension([&](Dimension& dimension) -> bool {
        auto const label = dimensionName(dimension.getDimensionId());
        if (query.dimension && label != *query.dimension) return true;
        auto const* chunkMap = dimension.getChunkSource().getChunkMap();
        if (!chunkMap) return true;
        for (auto const& [chunkPos, weakChunk] : *chunkMap) {
            if (!query.matchesChunk(label, chunkPos.x, chunkPos.z) || weakChunk.expired()) continue;
            result.emplace(chunkIdentity(label, chunkPos.x, chunkPos.z));
        }
        return true;
    });
    return result;
}

std::vector<model::ContainerRecord> RuntimeProvider::listContainers(
    ContainerQuery const& query,
    CancelCheck const& shouldCancel
) {
    std::vector<model::ContainerRecord> result;
    auto level = ll::service::getLevel();
    if (!level) {
        return result;
    }

    level->forEachDimension([&](Dimension& dimension) -> bool {
        if (shouldCancel && shouldCancel()) {
            return false;
        }
        auto const dimensionId = dimension.getDimensionId();
        auto const dimensionLabel = dimensionName(dimensionId);
        if (query.dimension && dimensionLabel != *query.dimension) return true;
        auto const* chunkMap = dimension.getChunkSource().getChunkMap();
        if (!chunkMap) {
            return true;
        }

        for (auto const& [chunkPos, weakChunk] : *chunkMap) {
            if (shouldCancel && shouldCancel()) {
                return false;
            }
            if (!query.matchesChunk(dimensionLabel, chunkPos.x, chunkPos.z)) continue;
            auto chunk = weakChunk.lock();
            if (!chunk) {
                continue;
            }

            for (auto const& [localPos, blockActor] : chunk->mBlockEntities.get().mMap.get()) {
                if (shouldCancel && shouldCancel()) {
                    return false;
                }
                (void)localPos;
                if (!blockActor) {
                    continue;
                }
                auto* container = blockActor->getContainer();
                if (!container) {
                    continue;
                }

                auto const& pos = blockActor->mPosition.get();
                model::ContainerRecord record;
                record.source = "runtime";
                record.kind = blockActorKind(blockActor->mType);
                record.dimension = dimensionLabel;
                record.position = {
                    static_cast<float>(pos.x),
                    static_cast<float>(pos.y),
                    static_cast<float>(pos.z),
                };
                record.chunkX = chunkPos.x;
                record.chunkZ = chunkPos.z;
                if (!query.matches(record)) continue;
                record.items = serialization::serializeContainer(*container);
                result.emplace_back(std::move(record));
            }

            for (auto const& weakActor : chunk->mEntities.get()) {
                if (shouldCancel && shouldCancel()) {
                    return false;
                }
                auto actor = weakActor.tryUnwrap<Actor>();
                if (!actor || actor->isPlayer()) {
                    continue;
                }
                auto component = actor->getEntityContext().tryGetComponent<ContainerComponent>();
                if (!component) {
                    continue;
                }

                auto const& pos = actor->getPosition();
                model::ContainerRecord record;
                record.source = "runtime";
                record.kind = "entity:" + actor->getTypeName();
                record.dimension = dimensionLabel;
                record.position = {pos.x, pos.y, pos.z};
                record.chunkX = chunkPos.x;
                record.chunkZ = chunkPos.z;
                if (!query.matches(record)) continue;
                record.items = serialization::serializeContainer(component->mContainer.get());
                result.emplace_back(std::move(record));
            }
        }
        return true;
    });

    return result;
}

} // namespace fullindex::providers
