#include "providers/RuntimeProvider.h"

#include "serialization/ItemSerializer.h"

#include <ll/api/service/Bedrock.h>
#include <mc/deps/shared_types/legacy/EquipmentSlot.h>
#include <mc/platform/UUID.h>
#include <mc/world/Container.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/level/Level.h>

#include <array>
#include <string>
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

} // namespace

bool RuntimeProvider::available() const {
    return ll::service::getLevel().has_value();
}

std::vector<model::PlayerRecord> RuntimeProvider::listPlayers() {
    std::vector<model::PlayerRecord> result;

    auto level = ll::service::getLevel();
    if (!level) {
        return result;
    }

    level->forEachPlayer([&](Player& player) -> bool {
        model::PlayerRecord record;
        record.name = player.getRealName();
        record.xuid = player.getXuid();
        record.uuid = player.getUuid().asString();
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

std::vector<model::DropRecord> RuntimeProvider::listDrops() {
    // TODO(M1): 遍历已加载 Actor，筛选 ItemActor，并按坐标转换 Chunk。
    return {};
}

std::vector<model::EntityRecord> RuntimeProvider::listEntities() {
    // TODO(M1): 遍历已加载 Actor，按 typeName + 分类规则拆分普通生物/特殊实体。
    return {};
}

std::vector<model::ContainerRecord> RuntimeProvider::listContainers() {
    // TODO(M1): 遍历已加载 LevelChunk / BlockActor，并统一适配 Container。
    return {};
}

} // namespace fullindex::providers
