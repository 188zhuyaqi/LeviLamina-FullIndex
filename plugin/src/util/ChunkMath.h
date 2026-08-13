#pragma once

#include <cmath>
#include <cstdint>

namespace fullindex::util {

inline std::int32_t blockToChunk(float coordinate) {
    return static_cast<std::int32_t>(std::floor(static_cast<double>(coordinate) / 16.0));
}

} // namespace fullindex::util
