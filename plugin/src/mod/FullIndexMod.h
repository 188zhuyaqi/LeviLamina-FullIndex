#pragma once

#include "config/Config.h"
#include "index/IndexService.h"
#include "ws/GatewayClient.h"

#include <ll/api/mod/NativeMod.h>

#include <memory>

namespace fullindex {

class FullIndexMod {
public:
    static FullIndexMod& getInstance();

    FullIndexMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
    config::Config mConfig;
    index::IndexService mIndexService;
    std::unique_ptr<ws::GatewayClient> mGatewayClient;
};

} // namespace fullindex
