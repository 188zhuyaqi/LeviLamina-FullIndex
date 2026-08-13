#pragma once

#include "providers/RuntimeProvider.h"
#include "providers/StorageProvider.h"

#include <nlohmann/json.hpp>

namespace fullindex::index {

class IndexService {
public:
    void configure(bool enableRuntimeProvider, bool enableStorageProvider) {
        mEnableRuntimeProvider = enableRuntimeProvider;
        mEnableStorageProvider = enableStorageProvider;
    }

    nlohmann::json execute(
        std::string const& requestId,
        std::string const& action,
        nlohmann::json const& params,
        providers::CancelCheck const& shouldCancel = {}
    );

    nlohmann::json liveQuery(
        std::string const& kind,
        nlohmann::json const& filters,
        providers::CancelCheck const& shouldCancel = {}
    );

private:
    [[nodiscard]] bool runtimeAvailable() const {
        return mEnableRuntimeProvider && mRuntime.available();
    }
    [[nodiscard]] bool storageAvailable() const {
        return mEnableStorageProvider && mStorage.available();
    }

    bool mEnableRuntimeProvider{true};
    bool mEnableStorageProvider{true};
    providers::RuntimeProvider mRuntime;
    providers::StorageProvider mStorage;
};

} // namespace fullindex::index
