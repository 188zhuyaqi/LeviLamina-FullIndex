#include "serialization/ItemSerializer.h"

#include <mc/deps/nbt/CompoundTag.h>
#include <mc/deps/nbt/ListTag.h>
#include <mc/world/Container.h>
#include <mc/world/item/ItemStack.h>

#include <array>
#include <string_view>
#include <utility>

namespace fullindex::serialization {
namespace {

constexpr int kMaxContainerDepth = 8;

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
