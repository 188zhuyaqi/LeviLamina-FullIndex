#pragma once

#include "providers/RuntimeProvider.h"
#include "providers/StorageProvider.h"

#include <nlohmann/json.hpp>

namespace fullindex::index {

class IndexService {
public:
    nlohmann::json execute(std::string const& requestId, std::string const& action, nlohmann::json const& params);

private:
    providers::RuntimeProvider mRuntime;
    providers::StorageProvider mStorage;
};

} // namespace fullindex::index
