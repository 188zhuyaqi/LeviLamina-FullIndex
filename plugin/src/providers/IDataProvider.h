#pragma once

#include "model/Records.h"

#include <functional>
#include <string_view>
#include <vector>

namespace fullindex::providers {

using CancelCheck = std::function<bool()>;

class IDataProvider {
public:
    virtual ~IDataProvider() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    [[nodiscard]] virtual bool available() const = 0;

    virtual std::vector<model::PlayerRecord> listPlayers(CancelCheck const& shouldCancel = {}) = 0;
    virtual std::vector<model::DropRecord> listDrops(CancelCheck const& shouldCancel = {}) = 0;
    virtual std::vector<model::EntityRecord> listEntities(CancelCheck const& shouldCancel = {}) = 0;
    virtual std::vector<model::ContainerRecord> listContainers(CancelCheck const& shouldCancel = {}) = 0;
};

} // namespace fullindex::providers
