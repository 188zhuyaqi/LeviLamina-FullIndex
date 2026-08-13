#include "index/IndexService.h"

#include "protocol/Protocol.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <chrono>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fullindex::index {
namespace {

struct PageSpec {
    std::size_t page{1};
    std::size_t pageSize{50};
};

PageSpec pageSpec(nlohmann::json const& params) {
    if (params.value("all", false)) {
        return {1, std::numeric_limits<std::size_t>::max()};
    }
    auto const requestedPage = params.value("page", 1);
    auto const requestedSize = params.value("pageSize", 50);
    return {
        static_cast<std::size_t>(std::max(requestedPage, 1)),
        static_cast<std::size_t>(std::clamp(requestedSize, 1, 200)),
    };
}

nlohmann::json pagedData(nlohmann::json const& allItems, PageSpec const& spec) {
    auto const total = allItems.size();
    auto const unbounded = spec.pageSize == std::numeric_limits<std::size_t>::max();
    auto const pageCount = total == 0 ? std::size_t{0} : (unbounded ? 1 : (total + spec.pageSize - 1) / spec.pageSize);
    auto const begin = std::min((spec.page - 1) * spec.pageSize, total);
    auto const end = unbounded ? total : std::min(begin + spec.pageSize, total);

    nlohmann::json items = nlohmann::json::array();
    for (auto index = begin; index < end; ++index) {
        items.push_back(allItems[index]);
    }

    return {
        {"items", std::move(items)},
        {"total", total},
        {"page", spec.page},
        {"pageSize", unbounded ? total : spec.pageSize},
        {"pageCount", pageCount},
        {"scope", "runtime_and_storage"},
        {"snapshotConsistent", false},
    };
}

struct DropItemGroup {
    std::string displayName;
    std::int64_t stackCount{};
    std::size_t entityCount{};
    nlohmann::json positions = nlohmann::json::array();
};

struct DropChunkGroup {
    std::string source{"storage"};
    std::size_t entityCount{};
    std::int64_t itemCount{};
    std::map<std::string, DropItemGroup> items;
};

nlohmann::json groupDrops(std::vector<model::DropRecord> const& drops) {
    using ChunkKey = std::tuple<std::string, std::int32_t, std::int32_t>;
    std::map<ChunkKey, DropChunkGroup> groups;

    for (auto const& drop : drops) {
        auto& group = groups[{drop.dimension, drop.chunkX, drop.chunkZ}];
        if (drop.source == "runtime") {
            group.source = "runtime";
        }
        ++group.entityCount;
        group.itemCount += drop.stackCount;

        auto& item = group.items[drop.itemId];
        item.displayName = drop.displayName;
        item.stackCount += drop.stackCount;
        ++item.entityCount;
        item.positions.push_back({
            {"x", drop.position.x},
            {"y", drop.position.y},
            {"z", drop.position.z},
        });
    }

    nlohmann::json rows = nlohmann::json::array();
    for (auto& [key, group] : groups) {
        nlohmann::json itemRows = nlohmann::json::array();
        for (auto& [itemId, item] : group.items) {
            itemRows.push_back({
                {"itemId", itemId},
                {"displayName", item.displayName},
                {"stackCount", item.stackCount},
                {"entityCount", item.entityCount},
                {"positions", std::move(item.positions)},
            });
        }
        std::sort(itemRows.begin(), itemRows.end(), [](auto const& left, auto const& right) {
            return left.at("stackCount").template get<std::int64_t>()
                > right.at("stackCount").template get<std::int64_t>();
        });

        auto const& [dimension, chunkX, chunkZ] = key;
        rows.push_back({
            {"source", group.source},
            {"dimension", dimension},
            {"chunkX", chunkX},
            {"chunkZ", chunkZ},
            {"entityCount", group.entityCount},
            {"itemCount", group.itemCount},
            {"distinctItemCount", group.items.size()},
            {"items", std::move(itemRows)},
        });
    }

    std::sort(rows.begin(), rows.end(), [](auto const& left, auto const& right) {
        auto const leftItems = left.at("itemCount").template get<std::int64_t>();
        auto const rightItems = right.at("itemCount").template get<std::int64_t>();
        if (leftItems != rightItems) {
            return leftItems > rightItems;
        }
        return left.at("entityCount").template get<std::size_t>()
            > right.at("entityCount").template get<std::size_t>();
    });
    return rows;
}

std::string normalizedIdentifier(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

void appendUnique(std::vector<std::string>& values, std::string const& value) {
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::vector<std::string> playerIdentifiers(model::PlayerRecord const& player) {
    std::vector<std::string> result;
    auto append = [&](std::string const& value) {
        if (!value.empty()) {
            appendUnique(result, normalizedIdentifier(value));
        }
    };
    append(player.xuid);
    append(player.uuid);
    for (auto const& id : player.storageIds) {
        append(id);
    }
    return result;
}

std::vector<model::PlayerRecord> mergedPlayers(std::vector<model::PlayerRecord> rows) {
    std::vector<std::size_t> parent(rows.size());
    std::iota(parent.begin(), parent.end(), 0);
    auto root = [&](std::size_t index) {
        while (parent[index] != index) {
            parent[index] = parent[parent[index]];
            index = parent[index];
        }
        return index;
    };
    auto unite = [&](std::size_t left, std::size_t right) {
        left = root(left);
        right = root(right);
        if (left != right) {
            parent[right] = left;
        }
    };

    std::unordered_map<std::string, std::size_t> identifierOwners;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        for (auto const& identifier : playerIdentifiers(rows[index])) {
            auto const [iterator, inserted] = identifierOwners.emplace(identifier, index);
            if (!inserted) {
                unite(index, iterator->second);
            }
        }
    }

    std::unordered_map<std::size_t, model::PlayerRecord> groups;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        auto& incoming = rows[index];
        auto const group = root(index);
        auto iterator = groups.find(group);
        if (iterator == groups.end()) {
            groups.emplace(group, std::move(incoming));
            continue;
        }

        auto& current = iterator->second;
        auto aliases = current.storageIds;
        for (auto const& id : incoming.storageIds) {
            appendUnique(aliases, id);
        }
        auto const realName = !incoming.realName.empty() ? incoming.realName : current.realName;
        auto const xuid = !incoming.xuid.empty() ? incoming.xuid : current.xuid;
        auto const uuid = !incoming.uuid.empty() ? incoming.uuid : current.uuid;

        // 在线记录含有 BDS 已认证的名称和最新背包/坐标，作为同一身份的主记录。
        if (incoming.online && !current.online) {
            current = std::move(incoming);
        }
        current.online = current.online || incoming.online;
        current.realName = realName;
        current.xuid = xuid;
        current.uuid = uuid;
        current.storageIds = std::move(aliases);
    }

    std::vector<model::PlayerRecord> result;
    result.reserve(groups.size());
    for (auto& [key, player] : groups) {
        (void)key;
        std::sort(player.storageIds.begin(), player.storageIds.end());
        if (!player.realName.empty()) {
            player.name = player.realName;
        } else if (!player.xuid.empty()) {
            player.name = player.xuid;
        } else if (!player.uuid.empty()) {
            player.name = player.uuid;
        } else if (!player.storageIds.empty()) {
            player.name = player.storageIds.front();
        }
        result.emplace_back(std::move(player));
    }
    return result;
}

std::string positionKey(
    std::string const& dimension,
    std::int32_t chunkX,
    std::int32_t chunkZ,
    model::Vec3Record const& position,
    std::string const& type
) {
    return dimension + ":" + std::to_string(chunkX) + ":" + std::to_string(chunkZ) + ":"
        + std::to_string(static_cast<int>(position.x * 10.0F)) + ":"
        + std::to_string(static_cast<int>(position.y * 10.0F)) + ":"
        + std::to_string(static_cast<int>(position.z * 10.0F)) + ":" + type;
}

std::vector<model::DropRecord> mergedDrops(
    std::vector<model::DropRecord> storageRows,
    std::vector<model::DropRecord> runtimeRows
) {
    std::unordered_map<std::string, model::DropRecord> rows;
    for (auto& row : storageRows) {
        rows[positionKey(row.dimension, row.chunkX, row.chunkZ, row.position, row.itemId)] = std::move(row);
    }
    for (auto& row : runtimeRows) {
        rows[positionKey(row.dimension, row.chunkX, row.chunkZ, row.position, row.itemId)] = std::move(row);
    }
    std::vector<model::DropRecord> result;
    result.reserve(rows.size());
    for (auto& [key, row] : rows) {
        (void)key;
        result.emplace_back(std::move(row));
    }
    return result;
}

std::vector<model::EntityRecord> mergedEntities(
    std::vector<model::EntityRecord> storageRows,
    std::vector<model::EntityRecord> runtimeRows
) {
    std::unordered_map<std::string, model::EntityRecord> rows;
    for (auto& row : storageRows) {
        rows[positionKey(row.dimension, row.chunkX, row.chunkZ, row.position, row.typeName)] = std::move(row);
    }
    for (auto& row : runtimeRows) {
        rows[positionKey(row.dimension, row.chunkX, row.chunkZ, row.position, row.typeName)] = std::move(row);
    }
    std::vector<model::EntityRecord> result;
    result.reserve(rows.size());
    for (auto& [key, row] : rows) {
        (void)key;
        result.emplace_back(std::move(row));
    }
    return result;
}

std::vector<model::ContainerRecord> mergedContainers(
    std::vector<model::ContainerRecord> storageRows,
    std::vector<model::ContainerRecord> runtimeRows
) {
    std::unordered_map<std::string, model::ContainerRecord> rows;
    auto keyFor = [](model::ContainerRecord const& row) {
        if (row.kind.starts_with("entity:")) {
            return positionKey(row.dimension, row.chunkX, row.chunkZ, row.position, row.kind);
        }
        return row.dimension + ":" + std::to_string(static_cast<int>(row.position.x)) + ":"
            + std::to_string(static_cast<int>(row.position.y)) + ":"
            + std::to_string(static_cast<int>(row.position.z));
    };
    for (auto& row : storageRows) {
        rows[keyFor(row)] = std::move(row);
    }
    for (auto& row : runtimeRows) {
        rows[keyFor(row)] = std::move(row);
    }
    std::vector<model::ContainerRecord> result;
    result.reserve(rows.size());
    for (auto& [key, row] : rows) {
        (void)key;
        result.emplace_back(std::move(row));
    }
    return result;
}

std::string lowerText(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string jsonText(nlohmann::json const& value, char const* key) {
    auto const iterator = value.find(key);
    return iterator != value.end() && iterator->is_string() ? iterator->get<std::string>() : "";
}

bool containsText(std::string const& value, std::string const& keyword) {
    return keyword.empty() || lowerText(value).find(keyword) != std::string::npos;
}

bool hasFilter(nlohmann::json const& filters, char const* key) {
    auto const iterator = filters.find(key);
    if (iterator == filters.end() || iterator->is_null()) return false;
    return !iterator->is_string() || !iterator->get<std::string>().empty();
}

std::optional<double> optionalNumber(nlohmann::json const& value, char const* key) {
    if (!hasFilter(value, key)) return std::nullopt;
    try {
        auto const& number = value.at(key);
        return number.is_number() ? number.get<double>() : std::stod(number.get<std::string>());
    } catch (...) {
        return std::nullopt;
    }
}

providers::ContainerQuery containerQuery(nlohmann::json const& filters) {
    providers::ContainerQuery query;
    if (hasFilter(filters, "dimension")) query.dimension = jsonText(filters, "dimension");
    if (auto value = optionalNumber(filters, "chunkX")) query.chunkX = static_cast<std::int32_t>(*value);
    if (auto value = optionalNumber(filters, "chunkZ")) query.chunkZ = static_cast<std::int32_t>(*value);
    query.x = optionalNumber(filters, "x");
    query.y = optionalNumber(filters, "y");
    query.z = optionalNumber(filters, "z");
    // X/Z 已经足以把查询收窄到一个区块；Y 只在区块内部做精确过滤。
    if (!query.hasChunk() && query.x && query.z) {
        query.chunkX = static_cast<std::int32_t>(std::floor(*query.x)) >> 4;
        query.chunkZ = static_cast<std::int32_t>(std::floor(*query.z)) >> 4;
    }
    return query;
}

std::int64_t epochMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

bool booleanFilter(nlohmann::json const& filters, char const* key) {
    auto const& value = filters.at(key);
    return value.is_boolean() ? value.get<bool>() : value.get<std::string>() == "true";
}

double numericFilter(nlohmann::json const& filters, char const* key) {
    auto const& value = filters.at(key);
    if (value.is_number()) return value.get<double>();
    return std::stod(value.get<std::string>());
}

double numericValue(nlohmann::json const& row, char const* key) {
    auto const iterator = row.find(key);
    if (iterator == row.end() || iterator->is_null()) return 0.0;
    return iterator->is_number() ? iterator->get<double>() : std::stod(iterator->get<std::string>());
}

bool matchesRange(
    nlohmann::json const& row,
    nlohmann::json const& filters,
    char const* field,
    char const* minimum,
    char const* maximum
) {
    auto const constrained = hasFilter(filters, minimum) || hasFilter(filters, maximum);
    auto const iterator = row.find(field);
    if (constrained && (iterator == row.end() || iterator->is_null())) return false;
    auto const value = numericValue(row, field);
    if (hasFilter(filters, minimum) && value < numericFilter(filters, minimum)) return false;
    if (hasFilter(filters, maximum) && value > numericFilter(filters, maximum)) return false;
    return true;
}

bool itemTreeNameContains(nlohmann::json const& items, std::string const& keyword) {
    if (keyword.empty()) return true;
    for (auto const& item : items) {
        if (containsText(jsonText(item, "displayName"), keyword)
            || containsText(jsonText(item, "customName"), keyword)) {
            return true;
        }
        auto const children = item.value("children", nlohmann::json::array());
        if (itemTreeNameContains(children, keyword)) return true;
    }
    return false;
}

bool matchesLiveRow(
    std::string const& kind,
    nlohmann::json const& row,
    nlohmann::json const& filters
) {
    auto const keyword = lowerText(jsonText(filters, "keyword"));
    if (hasFilter(filters, "dimension") && jsonText(row, "dimension") != jsonText(filters, "dimension")) return false;
    if (hasFilter(filters, "source") && jsonText(row, "source") != jsonText(filters, "source")) return false;
    if (hasFilter(filters, "chunkX") && numericValue(row, "chunkX") != numericFilter(filters, "chunkX")) return false;
    if (hasFilter(filters, "chunkZ") && numericValue(row, "chunkZ") != numericFilter(filters, "chunkZ")) return false;
    auto const position = row.value("position", nlohmann::json::object());
    if (hasFilter(filters, "x") && numericValue(position, "x") != numericFilter(filters, "x")) return false;
    if (hasFilter(filters, "y") && numericValue(position, "y") != numericFilter(filters, "y")) return false;
    if (hasFilter(filters, "z") && numericValue(position, "z") != numericFilter(filters, "z")) return false;

    if (kind == "players") {
        if (hasFilter(filters, "online") && row.value("online", false) != booleanFilter(filters, "online")) return false;
        if (keyword.empty()) return true;
        if (containsText(jsonText(row, "name"), keyword)
            || containsText(jsonText(row, "realName"), keyword)
            || containsText(jsonText(row, "xuid"), keyword)
            || containsText(jsonText(row, "uuid"), keyword)) return true;
        for (auto const& id : row.value("storageIds", nlohmann::json::array())) {
            if (id.is_string() && containsText(id.get<std::string>(), keyword)) return true;
        }
        return false;
    }
    if (kind == "containers") {
        if (hasFilter(filters, "typeId") && jsonText(row, "kind") != jsonText(filters, "typeId")) return false;
        auto const name = lowerText(jsonText(filters, "name"));
        return name.empty() || itemTreeNameContains(row.value("items", nlohmann::json::array()), name);
    }
    if (kind == "drops") {
        auto const typeId = jsonText(filters, "typeId");
        auto const name = lowerText(jsonText(filters, "name"));
        for (auto const& item : row.value("items", nlohmann::json::array())) {
            auto const typeMatches = typeId.empty() || jsonText(item, "itemId") == typeId;
            auto const nameMatches = name.empty() || containsText(jsonText(item, "displayName"), name);
            if (typeMatches && nameMatches) return true;
        }
        return typeId.empty() && name.empty();
    }
    if (kind == "entities") {
        if (hasFilter(filters, "category") && jsonText(row, "category") != jsonText(filters, "category")) return false;
        if (hasFilter(filters, "typeId") && jsonText(row, "typeName") != jsonText(filters, "typeId")) return false;
        auto const name = lowerText(jsonText(filters, "name"));
        return name.empty() || containsText(jsonText(row, "customName"), name);
    }
    return false;
}

nlohmann::json filterLiveRows(
    std::string const& kind,
    nlohmann::json const& rows,
    nlohmann::json const& filters,
    providers::CancelCheck const& shouldCancel
) {
    nlohmann::json result = nlohmann::json::array();
    for (auto const& row : rows) {
        if (shouldCancel && shouldCancel()) break;
        if (matchesLiveRow(kind, row, filters)) result.push_back(row);
    }
    return result;
}

nlohmann::json positionField(nlohmann::json const& position, char const* key) {
    auto const iterator = position.find(key);
    return iterator == position.end() ? nlohmann::json(nullptr) : *iterator;
}

void appendItemRows(
    nlohmann::json const& items,
    std::string const& sourceType,
    std::string const& owner,
    std::string const& dimension,
    nlohmann::json const& position,
    nlohmann::json const& chunkX,
    nlohmann::json const& chunkZ,
    std::string const& prefix,
    nlohmann::json& result,
    providers::CancelCheck const& shouldCancel
) {
    for (auto const& item : items) {
        if (shouldCancel && shouldCancel()) return;
        auto slot = jsonText(item, "slotName");
        if (slot.empty() && item.contains("slot")) slot = std::to_string(item.value("slot", 0));
        auto const itemPath = prefix.empty() ? slot : prefix + "/" + slot;
        result.push_back({
            {"source_type", sourceType}, {"owner", owner}, {"item_path", itemPath},
            {"item_id", jsonText(item, "id")}, {"display_name", jsonText(item, "displayName")},
            {"count", item.value("count", 0)}, {"dimension", dimension},
            {"x", positionField(position, "x")}, {"y", positionField(position, "y")},
            {"z", positionField(position, "z")}, {"chunk_x", chunkX}, {"chunk_z", chunkZ},
            {"enchanted", item.value("enchanted", false)},
            {"has_container_data", item.value("hasContainerData", false)},
            {"custom_name", jsonText(item, "customName")}, {"damage", item.value("damage", 0)},
            {"detail", item},
        });
        auto const childPrefix = itemPath + ":" + jsonText(item, "id");
        appendItemRows(
            item.value("children", nlohmann::json::array()), sourceType, owner, dimension,
            position, chunkX, chunkZ, childPrefix, result, shouldCancel
        );
    }
}

bool matchesItemRow(nlohmann::json const& row, nlohmann::json const& filters) {
    if (hasFilter(filters, "typeId") && jsonText(row, "item_id") != jsonText(filters, "typeId")) return false;
    auto const name = lowerText(jsonText(filters, "name"));
    if (!name.empty() && !containsText(jsonText(row, "display_name"), name)
        && !containsText(jsonText(row, "custom_name"), name)) return false;
    if (hasFilter(filters, "sourceType") && jsonText(row, "source_type") != jsonText(filters, "sourceType")) return false;
    if (hasFilter(filters, "dimension") && jsonText(row, "dimension") != jsonText(filters, "dimension")) return false;
    if (hasFilter(filters, "owner")
        && !containsText(jsonText(row, "owner"), lowerText(jsonText(filters, "owner")))) return false;
    if (hasFilter(filters, "itemPath")
        && !containsText(jsonText(row, "item_path"), lowerText(jsonText(filters, "itemPath")))) return false;
    if (hasFilter(filters, "enchanted") && row.value("enchanted", false) != booleanFilter(filters, "enchanted")) return false;
    if (hasFilter(filters, "hasContainerData")
        && row.value("has_container_data", false) != booleanFilter(filters, "hasContainerData")) return false;
    if (hasFilter(filters, "nestedOnly") && booleanFilter(filters, "nestedOnly")
        && jsonText(row, "item_path").find(':') == std::string::npos) return false;
    return matchesRange(row, filters, "count", "countMin", "countMax")
        && matchesRange(row, filters, "x", "xMin", "xMax")
        && matchesRange(row, filters, "y", "yMin", "yMax")
        && matchesRange(row, filters, "z", "zMin", "zMax")
        && matchesRange(row, filters, "chunk_x", "chunkXMin", "chunkXMax")
        && matchesRange(row, filters, "chunk_z", "chunkZMin", "chunkZMax");
}

nlohmann::json filterItemRows(
    nlohmann::json const& rows,
    nlohmann::json const& filters,
    providers::CancelCheck const& shouldCancel
) {
    nlohmann::json result = nlohmann::json::array();
    for (auto const& row : rows) {
        if (shouldCancel && shouldCancel()) break;
        if (matchesItemRow(row, filters)) result.push_back(row);
    }
    return result;
}

} // namespace

nlohmann::json IndexService::execute(
    std::string const& requestId,
    std::string const& action,
    nlohmann::json const& params,
    providers::CancelCheck const& shouldCancel
) {
    using namespace protocol;

    auto const scope = params.value("scope", "runtime_and_storage");
    auto const includeRuntime = scope != "storage";
    auto const includeStorage = scope != "runtime";

    if (action == "system.capabilities") {
        auto const runtime = runtimeAvailable();
        auto const storage = storageAvailable();
        return makeResponse(requestId, action, {
            {"runtime", runtime},
            {"storage", storage},
            {"features", {
                {"players", runtime || storage},
                {"playerInventory", runtime || storage},
                {"containers", runtime || storage},
                {"drops", runtime || storage},
                {"entities", runtime || storage},
                {"storageScan", storage},
                {"offlinePlayers", storage},
                {"unloadedContainers", storage},
                {"actorStorage", storage},
                {"nestedItems", runtime || storage},
                {"mutation", false}
            }}
        });
    }

    if (action == "players.list") {
        std::vector<model::PlayerRecord> collected;

        if (includeStorage && storageAvailable()) {
            for (auto& player : mStorage.listPlayers(shouldCancel)) {
                collected.emplace_back(std::move(player));
            }
        }

        if (includeRuntime && runtimeAvailable()) {
            for (auto& player : mRuntime.listPlayers(shouldCancel)) {
                collected.emplace_back(std::move(player));
            }
        }

        auto players = mergedPlayers(std::move(collected));
        std::sort(players.begin(), players.end(), [](auto const& left, auto const& right) {
            if (left.online != right.online) {
                return left.online > right.online;
            }
            return left.name < right.name;
        });

        nlohmann::json rows = nlohmann::json::array();
        for (auto const& player : players) {
            rows.push_back(playerToJson(player));
        }
        return makeResponse(requestId, action, {
            {"items", std::move(rows)},
            {"sourcePriority", {"runtime", "storage"}}
        });
    }

    if (action == "drops.list") {
        auto storageRows = includeStorage && storageAvailable() ? mStorage.listDrops(shouldCancel) : std::vector<model::DropRecord>{};
        auto runtimeRows = includeRuntime && runtimeAvailable() ? mRuntime.listDrops(shouldCancel) : std::vector<model::DropRecord>{};
        auto rows = groupDrops(mergedDrops(std::move(storageRows), std::move(runtimeRows)));
        auto data = pagedData(rows, pageSpec(params));
        data["groupedBy"] = {"dimension", "chunkX", "chunkZ"};
        data["sort"] = "itemCount:desc";
        data["sourcePriority"] = {"runtime", "storage"};
        return makeResponse(requestId, action, std::move(data));
    }

    if (action == "entities.list") {
        auto storageRows = includeStorage && storageAvailable() ? mStorage.listEntities(shouldCancel) : std::vector<model::EntityRecord>{};
        auto runtimeRows = includeRuntime && runtimeAvailable() ? mRuntime.listEntities(shouldCancel) : std::vector<model::EntityRecord>{};
        auto records = mergedEntities(std::move(storageRows), std::move(runtimeRows));
        std::sort(records.begin(), records.end(), [](auto const& left, auto const& right) {
            return std::tie(left.category, left.typeName, left.dimension, left.chunkX, left.chunkZ)
                < std::tie(right.category, right.typeName, right.dimension, right.chunkX, right.chunkZ);
        });
        nlohmann::json rows = nlohmann::json::array();
        for (auto const& entity : records) {
            rows.push_back(entityToJson(entity));
        }
        auto data = pagedData(rows, pageSpec(params));
        data["sourcePriority"] = {"runtime", "storage"};
        return makeResponse(requestId, action, std::move(data));
    }

    if (action == "containers.list") {
        auto storageRows = includeStorage && storageAvailable() ? mStorage.listContainers(shouldCancel) : std::vector<model::ContainerRecord>{};
        auto runtimeRows = includeRuntime && runtimeAvailable() ? mRuntime.listContainers(shouldCancel) : std::vector<model::ContainerRecord>{};
        auto records = mergedContainers(std::move(storageRows), std::move(runtimeRows));
        std::sort(records.begin(), records.end(), [](auto const& left, auto const& right) {
            return std::tie(left.dimension, left.chunkX, left.chunkZ, left.kind)
                < std::tie(right.dimension, right.chunkX, right.chunkZ, right.kind);
        });
        nlohmann::json rows = nlohmann::json::array();
        for (auto const& container : records) {
            rows.push_back(containerToJson(container));
        }
        auto data = pagedData(rows, pageSpec(params));
        data["sourcePriority"] = {"runtime", "storage"};
        return makeResponse(requestId, action, std::move(data));
    }

    if (action == "containers.get") {
        auto const query = containerQuery(params);
        if (!query.hasPoint() || !query.dimension) {
            return makeError(requestId, action, "dimension, x, y and z are required");
        }

        auto loadedChunks = runtimeAvailable()
            ? mRuntime.loadedContainerChunks(query)
            : std::unordered_set<std::string>{};
        auto storageRows = storageAvailable()
            ? mStorage.listContainers(query, loadedChunks, shouldCancel)
            : std::vector<model::ContainerRecord>{};
        auto runtimeRows = runtimeAvailable()
            ? mRuntime.listContainers(query, shouldCancel)
            : std::vector<model::ContainerRecord>{};
        auto records = mergedContainers(std::move(storageRows), std::move(runtimeRows));
        if (records.empty()) return makeError(requestId, action, "container not found at current world state");
        return makeResponse(requestId, action, {
            {"item", containerToJson(records.front())},
            {"readAt", epochMilliseconds()},
            {"persisted", false},
        });
    }

    return makeError(requestId, action, "unknown action");
}

nlohmann::json IndexService::liveQuery(
    std::string const& kind,
    nlohmann::json const& filters,
    providers::CancelCheck const& shouldCancel
) {
    auto const params = nlohmann::json{{"all", true}, {"scope", "runtime_and_storage"}};
    if (kind == "containers") {
        auto const query = containerQuery(filters);
        auto const requestedSource = jsonText(filters, "source");
        auto loadedChunks = runtimeAvailable()
            ? mRuntime.loadedContainerChunks(query)
            : std::unordered_set<std::string>{};
        auto storageRows = (requestedSource.empty() || requestedSource == "storage") && storageAvailable()
            ? mStorage.listContainers(query, loadedChunks, shouldCancel)
            : std::vector<model::ContainerRecord>{};
        auto runtimeRows = (requestedSource.empty() || requestedSource == "runtime") && runtimeAvailable()
            ? mRuntime.listContainers(query, shouldCancel)
            : std::vector<model::ContainerRecord>{};
        auto records = mergedContainers(std::move(storageRows), std::move(runtimeRows));
        nlohmann::json rows = nlohmann::json::array();
        for (auto const& record : records) {
            if (shouldCancel && shouldCancel()) break;
            auto row = protocol::containerToJson(record);
            if (!matchesLiveRow(kind, row, filters)) continue;
            row.erase("items");
            row["detailAvailable"] = true;
            rows.push_back(std::move(row));
        }
        return rows;
    }
    if (kind != "items") {
        auto const action = kind == "players" ? "players.list"
            : kind == "containers" ? "containers.list"
            : kind == "drops" ? "drops.list"
            : kind == "entities" ? "entities.list" : "";
        if (std::string(action).empty()) throw std::runtime_error("unsupported live query kind");
        auto response = execute("live-query", action, params, shouldCancel);
        if (!response.value("ok", false)) throw std::runtime_error(response.value("error", "live query failed"));
        return filterLiveRows(kind, response["data"]["items"], filters, shouldCancel);
    }

    nlohmann::json rows = nlohmann::json::array();
    auto const requestedSource = jsonText(filters, "sourceType");
    if (requestedSource.empty() || requestedSource == "player") {
      auto players = execute("live-query", "players.list", params, shouldCancel);
      if (players.value("ok", false)) {
        for (auto const& player : players["data"]["items"]) {
            auto const position = player.value("position", nlohmann::json::object());
            auto const owner = jsonText(player, "name");
            auto const dimension = jsonText(player, "dimension");
            appendItemRows(player.value("inventory", nlohmann::json::array()), "player", owner, dimension, position, nullptr, nullptr, "inventory", rows, shouldCancel);
            appendItemRows(player.value("armor", nlohmann::json::array()), "player", owner, dimension, position, nullptr, nullptr, "armor", rows, shouldCancel);
            if (player.contains("offhand") && !player["offhand"].is_null()) {
                appendItemRows(nlohmann::json::array({player["offhand"]}), "player", owner, dimension, position, nullptr, nullptr, "offhand", rows, shouldCancel);
            }
            appendItemRows(player.value("enderChest", nlohmann::json::array()), "player", owner, dimension, position, nullptr, nullptr, "ender_chest", rows, shouldCancel);
        }
      }
    }
    if (shouldCancel && shouldCancel()) return nlohmann::json::array();

    if (requestedSource.empty() || requestedSource == "container") {
      auto containers = execute("live-query", "containers.list", params, shouldCancel);
      if (containers.value("ok", false)) {
        for (auto const& container : containers["data"]["items"]) {
            auto const position = container.value("position", nlohmann::json::object());
            auto const owner = jsonText(container, "kind") + "@"
                + std::to_string(static_cast<int>(numericValue(position, "x"))) + ","
                + std::to_string(static_cast<int>(numericValue(position, "y"))) + ","
                + std::to_string(static_cast<int>(numericValue(position, "z")));
            appendItemRows(
                container.value("items", nlohmann::json::array()), "container", owner,
                jsonText(container, "dimension"), position,
                container.value("chunkX", nlohmann::json(nullptr)),
                container.value("chunkZ", nlohmann::json(nullptr)),
                "container", rows, shouldCancel
            );
        }
      }
    }
    if (shouldCancel && shouldCancel()) return nlohmann::json::array();

    if (requestedSource.empty() || requestedSource == "drop") {
      auto drops = execute("live-query", "drops.list", params, shouldCancel);
      if (drops.value("ok", false)) {
        for (auto const& chunk : drops["data"]["items"]) {
            auto const owner = "chunk:" + std::to_string(chunk.value("chunkX", 0)) + ","
                + std::to_string(chunk.value("chunkZ", 0));
            for (auto const& item : chunk.value("items", nlohmann::json::array())) {
                auto const positions = item.value("positions", nlohmann::json::array());
                auto const position = positions.empty() ? nlohmann::json::object() : positions.front();
                auto detail = item;
                rows.push_back({
                    {"source_type", "drop"}, {"owner", owner}, {"item_path", "drop"},
                    {"item_id", jsonText(item, "itemId")}, {"display_name", jsonText(item, "displayName")},
                    {"count", item.value("stackCount", 0)}, {"dimension", jsonText(chunk, "dimension")},
                    {"x", positionField(position, "x")}, {"y", positionField(position, "y")},
                    {"z", positionField(position, "z")},
                    {"chunk_x", chunk.value("chunkX", nlohmann::json(nullptr))},
                    {"chunk_z", chunk.value("chunkZ", nlohmann::json(nullptr))},
                    {"enchanted", false}, {"has_container_data", false}, {"custom_name", ""},
                    {"damage", 0}, {"detail", std::move(detail)},
                });
            }
        }
      }
    }
    return filterItemRows(rows, filters, shouldCancel);
}

} // namespace fullindex::index
