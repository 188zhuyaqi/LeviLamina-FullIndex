#include "providers/StorageProvider.h"

#include <ll/api/service/Bedrock.h>

namespace fullindex::providers {

bool StorageProvider::available() const {
    // getDBStorage 是后续 M2 的入口。这里仅用可用性标识，不在尚未验证序列化格式时贸然解析。
    return ll::service::getDBStorage() != nullptr;
}

std::vector<model::PlayerRecord> StorageProvider::listPlayers() {
    // TODO(M2): PlayerDataSystem / LevelStorage 离线玩家读取。
    return {};
}

std::vector<model::DropRecord> StorageProvider::listDrops() {
    // TODO(M2): DBChunkStorage ActorStorage -> ItemActor 持久化数据。
    return {};
}

std::vector<model::EntityRecord> StorageProvider::listEntities() {
    // TODO(M2): ActorStorage 反序列化与分类。
    return {};
}

std::vector<model::ContainerRecord> StorageProvider::listContainers() {
    // TODO(M2): 未加载 Chunk 的 BlockActor / Container 解析。
    return {};
}

} // namespace fullindex::providers
