#pragma once

#include "providers/ContainerQuery.h"
#include "providers/IDataProvider.h"

#include <unordered_set>

namespace fullindex::providers {

class RuntimeProvider final : public IDataProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "runtime"; }
    [[nodiscard]] bool available() const override;

    std::vector<model::PlayerRecord> listPlayers(CancelCheck const& shouldCancel = {}) override;
    std::vector<model::DropRecord> listDrops(CancelCheck const& shouldCancel = {}) override;
    std::vector<model::EntityRecord> listEntities(CancelCheck const& shouldCancel = {}) override;
    std::vector<model::ContainerRecord> listContainers(CancelCheck const& shouldCancel = {}) override;

    std::vector<model::ContainerRecord> listContainers(
        ContainerQuery const& query,
        CancelCheck const& shouldCancel = {}
    );
    std::unordered_set<std::string> loadedContainerChunks(ContainerQuery const& query = {});
};

} // namespace fullindex::providers
