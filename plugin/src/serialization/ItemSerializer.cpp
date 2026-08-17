#include "serialization/ItemSerializer.h"

#include <mc/deps/nbt/CompoundTag.h>
#include <mc/deps/nbt/ListTag.h>
#include <mc/world/Container.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/enchanting/EnchantUtils.h>
#include <mc/world/item/enchanting/EnchantmentInstance.h>
#include <mc/world/item/enchanting/ItemEnchants.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace fullindex::serialization {
namespace {

constexpr int kMaxContainerDepth = 8;

std::string_view enchantId(Enchant::Type type) {
    switch (type) {
    case Enchant::Type::Protection: return "protection";
    case Enchant::Type::FireProtection: return "fire_protection";
    case Enchant::Type::FeatherFalling: return "feather_falling";
    case Enchant::Type::BlastProtection: return "blast_protection";
    case Enchant::Type::ProjectileProtection: return "projectile_protection";
    case Enchant::Type::Thorns: return "thorns";
    case Enchant::Type::Respiration: return "respiration";
    case Enchant::Type::DepthStrider: return "depth_strider";
    case Enchant::Type::AquaAffinity: return "aqua_affinity";
    case Enchant::Type::Sharpness: return "sharpness";
    case Enchant::Type::Smite: return "smite";
    case Enchant::Type::BaneOfArthropods: return "bane_of_arthropods";
    case Enchant::Type::Knockback: return "knockback";
    case Enchant::Type::FireAspect: return "fire_aspect";
    case Enchant::Type::Looting: return "looting";
    case Enchant::Type::Efficiency: return "efficiency";
    case Enchant::Type::SilkTouch: return "silk_touch";
    case Enchant::Type::Unbreaking: return "unbreaking";
    case Enchant::Type::Fortune: return "fortune";
    case Enchant::Type::Power: return "power";
    case Enchant::Type::Punch: return "punch";
    case Enchant::Type::Flame: return "flame";
    case Enchant::Type::Infinity: return "infinity";
    case Enchant::Type::LuckOfTheSea: return "luck_of_the_sea";
    case Enchant::Type::Lure: return "lure";
    case Enchant::Type::FrostWalker: return "frost_walker";
    case Enchant::Type::Mending: return "mending";
    case Enchant::Type::CurseOfBinding: return "binding_curse";
    case Enchant::Type::CurseOfVanishing: return "vanishing_curse";
    case Enchant::Type::Impaling: return "impaling";
    case Enchant::Type::Riptide: return "riptide";
    case Enchant::Type::Loyalty: return "loyalty";
    case Enchant::Type::Channeling: return "channeling";
    case Enchant::Type::Multishot: return "multishot";
    case Enchant::Type::Piercing: return "piercing";
    case Enchant::Type::QuickCharge: return "quick_charge";
    case Enchant::Type::SoulSpeed: return "soul_speed";
    case Enchant::Type::SwiftSneak: return "swift_sneak";
    case Enchant::Type::WindBurst: return "wind_burst";
    case Enchant::Type::Density: return "density";
    case Enchant::Type::Breach: return "breach";
    case Enchant::Type::Lunge: return "lunge";
    default: return "unknown";
    }
}

std::string fallbackLevelText(int level) {
    if (level <= 0 || level > 3999) {
        return std::to_string(level);
    }

    constexpr std::array<std::pair<int, std::string_view>, 13> numerals{{
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"}, {50, "L"},
        {40, "XL"}, {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"},
    }};

    std::string result;
    for (auto const& [value, numeral] : numerals) {
        while (level >= value) {
            result += numeral;
            level -= value;
        }
    }
    return result;
}

std::vector<model::EnchantmentRecord> serializeEnchantments(ItemStack const& stack) {
    auto const itemEnchants = stack.constructItemEnchantsFromUserData();
    auto const allEnchants = itemEnchants.getAllEnchants();
    std::vector<model::EnchantmentRecord> result;
    result.reserve(allEnchants.size());

    for (auto const& enchantment : allEnchants) {
        auto const type = static_cast<Enchant::Type>(enchantment.mEnchantType);
        auto const level = static_cast<int>(enchantment.mLevel);
        auto levelText = fallbackLevelText(level);
        auto const id = enchantId(type);
        result.push_back({
            static_cast<std::int32_t>(type),
            id == "unknown" ? "minecraft:enchantment_" + std::to_string(static_cast<int>(type))
                            : "minecraft:" + std::string(id),
            EnchantUtils::getEnchantNameAndLevel(type, level),
            level,
            std::move(levelText),
        });
    }
    return result;
}

int itemSlot(CompoundTag const& tag, int fallback) {
    auto const iterator = tag.mTags.find("Slot");
    if (iterator == tag.mTags.end() || !iterator->second.is_number_integer()) {
        return fallback;
    }
    return static_cast<int>(iterator->second);
}

model::ItemRecord serializeItemImpl(ItemStack const& stack, int slot, std::string slotName, int depth);

void appendItemList(model::ItemRecord& parent, ListTag const& list, int depth) {
    if (depth >= kMaxContainerDepth) {
        return;
    }

    for (std::size_t index = 0; index < list.size(); ++index) {
        auto const& entry = list[index];
        if (!entry || !entry.hold<CompoundTag>()) {
            continue;
        }
        auto const& itemTag = entry.get<CompoundTag>();
        if (!itemTag.contains("Name", Tag::String) && !itemTag.contains("name", Tag::String)) {
            continue;
        }
        auto stack = ItemStack::fromTag(itemTag);
        if (!stack.isNull()) {
            parent.children.emplace_back(
                serializeItemImpl(stack, itemSlot(itemTag, static_cast<int>(index)), "container", depth + 1)
            );
        }
    }
}

void findNestedItems(model::ItemRecord& parent, CompoundTag const& tag, int depth) {
    if (depth >= kMaxContainerDepth) {
        return;
    }

    constexpr std::array<std::string_view, 6> kItemListNames{
        "Items",
        "items",
        "StorageItems",
        "storage_item_component_content",
        "bundle_contents",
        "minecraft:bundle_contents",
    };

    for (auto const name : kItemListNames) {
        auto const iterator = tag.mTags.find(name);
        if (iterator != tag.mTags.end() && iterator->second.hold<ListTag>()) {
            appendItemList(parent, iterator->second.get<ListTag>(), depth);
        }
    }

    for (auto const& [name, value] : tag.mTags) {
        (void)name;
        if (value.hold<CompoundTag>()) {
            findNestedItems(parent, value.get<CompoundTag>(), depth + 1);
        }
    }
}

model::ItemRecord serializeItemImpl(ItemStack const& stack, int slot, std::string slotName, int depth) {
    model::ItemRecord item;
    item.slot = slot;
    item.slotName = std::move(slotName);

    if (stack.isNull()) {
        return item;
    }

    item.id = stack.getTypeName();
    item.displayName = stack.getDescriptionName();
    item.count = static_cast<std::int32_t>(stack.mCount);
    item.aux = static_cast<std::int32_t>(stack.getAuxValue());
    item.damage = static_cast<std::int32_t>(stack.getDamageValue());
    item.enchanted = stack.isEnchanted();
    item.hasContainerData = stack.hasContainerData();
    item.customName = stack.getCustomName();
    item.lore = stack.getCustomLore();
    item.enchantments = serializeEnchantments(stack);

    if (stack.mUserData && depth < kMaxContainerDepth) {
        findNestedItems(item, *stack.mUserData, depth);
    }
    return item;
}

} // namespace

model::ItemRecord serializeItem(ItemStack const& stack, int slot, std::string slotName) {
    return serializeItemImpl(stack, slot, std::move(slotName), 0);
}

std::vector<model::ItemRecord> serializeContainer(Container const& container) {
    std::vector<model::ItemRecord> items;
    auto const size = container.getContainerSize();
    items.reserve(static_cast<std::size_t>(size));

    for (int slot = 0; slot < size; ++slot) {
        auto const& stack = container.getItem(slot);
        if (stack.isNull()) {
            continue;
        }
        items.emplace_back(serializeItem(stack, slot));
    }

    return items;
}

} // namespace fullindex::serialization
