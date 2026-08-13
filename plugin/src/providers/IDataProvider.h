#pragma once

#include "model/Records.h"

#include <string_view>
#include <vector>

namespace fullindex::providers {

class IDataProvider {
public:
    virtual ~IDataProvider() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual bool available() const = 0;

    virtual std::vector<model::PlayerRecord> listPlayers() = 0;
    virtual std::vector<model::DropRecord> listDrops() = 0;
    virtual std::vector<model::EntityRecord> listEntities() = 0;
    virtual std::vector<model::ContainerRecord> listContainers() = 0;
};

} // namespace fullindex::providers
