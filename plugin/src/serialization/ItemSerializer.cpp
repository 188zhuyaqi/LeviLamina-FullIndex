#include "serialization/ItemSerializer.h"

#include <mc/world/Container.h>
#include <mc/world/item/ItemStack.h>

#include <utility>

namespace fullindex::serialization {

model::ItemRecord serializeItem(ItemStack const& stack, int slot, std::string slotName) {
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

    // M2: 对 hasContainerData() 的潜影盒、收纳袋等读取内部 ContainerComponent/NBT，
    // 并递归填充 children。这里先保留标记，避免未经验证直接猜存档结构。
    return item;
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
