#pragma once

#include "model/Records.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace fullindex::providers {

struct ContainerQuery {
    std::optional<std::string> dimension;
    std::optional<std::int32_t> chunkX;
    std::optional<std::int32_t> chunkZ;
    std::optional<double> x;
    std::optional<double> y;
    std::optional<double> z;

    [[nodiscard]] bool hasPoint() const { return x && y && z; }
    [[nodiscard]] bool hasChunk() const { return chunkX && chunkZ; }

    [[nodiscard]] bool matchesChunk(std::string const& valueDimension, std::int32_t valueX, std::int32_t valueZ) const {
        if (dimension && valueDimension != *dimension) return false;
        if (chunkX && valueX != *chunkX) return false;
        if (chunkZ && valueZ != *chunkZ) return false;
        return true;
    }

    [[nodiscard]] bool matches(model::ContainerRecord const& row) const {
        if (!matchesChunk(row.dimension, row.chunkX, row.chunkZ)) return false;
        if (x && std::fabs(static_cast<double>(row.position.x) - *x) > 0.001) return false;
        if (y && std::fabs(static_cast<double>(row.position.y) - *y) > 0.001) return false;
        if (z && std::fabs(static_cast<double>(row.position.z) - *z) > 0.001) return false;
        return true;
    }
};

} // namespace fullindex::providers
