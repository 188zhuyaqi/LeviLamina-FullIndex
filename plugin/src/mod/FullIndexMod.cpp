#include "mod/FullIndexMod.h"

#include "ll/api/mod/RegisterHelper.h"

namespace fullindex {

FullIndexMod& FullIndexMod::getInstance() {
    static FullIndexMod instance;
    return instance;
}

bool FullIndexMod::load() {
    auto& logger = getSelf().getLogger();
    logger.info("FullIndex loading...");

    if (!config::loadOrCreate(mConfig)) {
        logger.error("Failed to load/create plugins/FullIndex/config.json");
        return false;
    }

    if (mConfig.pluginToken == "CHANGE_ME") {
        logger.warn("pluginToken is still CHANGE_ME. Set a strong token before exposing the gateway.");
    }

    mIndexService.configure(mConfig.enableRuntimeProvider, mConfig.enableStorageProvider);

    return true;
}

bool FullIndexMod::enable() {
    auto& logger = getSelf().getLogger();

    mGatewayClient = std::make_unique<ws::GatewayClient>(mConfig, mIndexService);
    mGatewayClient->start();

    logger.info("FullIndex enabled. Gateway: {}", mConfig.gatewayUrl);
    return true;
}

bool FullIndexMod::disable() {
    if (mGatewayClient) {
        mGatewayClient->stop();
        mGatewayClient.reset();
    }

    getSelf().getLogger().info("FullIndex disabled.");
    return true;
}

} // namespace fullindex

LL_REGISTER_MOD(fullindex::FullIndexMod, fullindex::FullIndexMod::getInstance());
