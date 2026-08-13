#pragma once

#include "providers/IDataProvider.h"

namespace fullindex::providers {

// StorageProvider 只通过 BDS 自己持有的 DBStorage / LevelStorage 访问世界数据。
// 禁止在服务器运行时另开 worlds/<level>/db 的第二个 LevelDB 句柄。
class StorageProvider final : public IDataProvider {
public:
    [[nodiscard]] std::string_view name() const override { return "storage"; }
    [[nodiscard]] bool available() const override;

    std::vector<model::PlayerRecord> listPlayers() override;
    std::vector<model::DropRecord> listDrops() override;
    std::vector<model::EntityRecord> listEntities() override;
    std::vector<model::ContainerRecord> listContainers() override;
};

} // namespace fullindex::providers
