#pragma once

#include "providers/ContainerQuery.h"
#include "providers/IDataProvider.h"

#include <unordered_set>

namespace fullindex::providers {

// StorageProvider 只通过 BDS 自己持有的 DBStorage / LevelStorage 访问世界数据。
// 禁止在服务器运行时另开 worlds/<level>/db 的第二个 LevelDB 句柄。
class StorageProvider final : public IDataProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "storage"; }
    [[nodiscard]] bool available() const override;

    std::vector<model::PlayerRecord> listPlayers(CancelCheck const& shouldCancel = {}) override;
    std::vector<model::DropRecord> listDrops(CancelCheck const& shouldCancel = {}) override;
    std::vector<model::EntityRecord> listEntities(CancelCheck const& shouldCancel = {}) override;
    std::vector<model::ContainerRecord> listContainers(CancelCheck const& shouldCancel = {}) override;

    std::vector<model::ContainerRecord> listContainers(
        ContainerQuery const& query,
        std::unordered_set<std::string> const& excludedChunks,
        CancelCheck const& shouldCancel = {}
    );
};

} // namespace fullindex::providers
