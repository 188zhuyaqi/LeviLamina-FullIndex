#include "providers/RuntimeProvider.h"

#include <ll/api/service/Bedrock.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>

namespace fullindex::providers {

bool RuntimeProvider::available() const {
    return ll::service::getLevel().has_value();
}

std::vector<model::PlayerRecord> RuntimeProvider::listPlayers() {
    std::vector<model::PlayerRecord> result;

    auto level = ll::service::getLevel();
    if (!level) {
        return result;
    }

    // v26.10.11: getLevel() 返回 optional_ref<Level>，已加载对象以 Runtime 为权威源。
    // Inventory / Armor / EnderChest 在下一步逐项按 v26.10.11 头文件补齐。
    level->forEachPlayer([&](Player& player) -> bool {
        model::PlayerRecord record;
        record.name = player.getRealName();
        record.xuid = player.getXuid();
        record.online = true;

        auto const& pos = player.getPosition();
        record.position = {pos.x, pos.y, pos.z};

        result.emplace_back(std::move(record));
        return true;
    });

    return result;
}

std::vector<model::DropRecord> RuntimeProvider::listDrops() {
    // TODO(M1): 遍历已加载 Actor，筛选 ItemActor，并按坐标转换 Chunk。
    return {};
}

std::vector<model::EntityRecord> RuntimeProvider::listEntities() {
    // TODO(M1): 遍历已加载 Actor，按 typeName + 分类规则拆分普通生物/特殊实体。
    return {};
}

std::vector<model::ContainerRecord> RuntimeProvider::listContainers() {
    // TODO(M1): 遍历已加载 LevelChunk / BlockActor，并统一适配 Container。
    return {};
}

} // namespace fullindex::providers
