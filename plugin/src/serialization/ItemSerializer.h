#pragma once

#include "model/Records.h"

#include <string>
#include <vector>

class Container;
class ItemStack;

namespace fullindex::serialization {

model::ItemRecord serializeItem(ItemStack const& stack, int slot = -1, std::string slotName = {});
std::vector<model::ItemRecord> serializeContainer(Container const& container);

} // namespace fullindex::serialization
