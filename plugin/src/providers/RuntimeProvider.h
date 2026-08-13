#pragma once

#include "providers/IDataProvider.h"

namespace fullindex::providers {

class RuntimeProvider final : public IDataProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "runtime"; }
    [[nodiscard]] bool available() const override;

    std::vector<model::PlayerRecord> listPlayers() override;
    std::vector<model::DropRecord> listDrops() override;
    std::vector<model::EntityRecord> listEntities() override;
    std::vector<model::ContainerRecord> listContainers() override;
};

} // namespace fullindex::providers
