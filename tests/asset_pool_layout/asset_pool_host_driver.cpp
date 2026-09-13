#include "app.h"
#include "libs/asset_pool_layout/asset_pool_layout.h"
#include <cstdio>
#include <cstring>
#include <memory>

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    if (std::strcmp(argv[1], "setup-failure") == 0)
    {
        unsigned injectionCalls = 0, halCalls = 0;
        APP::SetupCallback_t callback;
        callback.HalInjection = [&]() { ++halCalls; };
        if (APP::Setup(callback) || halCalls != 0)
            return 1;
        callback.AssetPoolInjection = [&]() -> bool
        {
            ++injectionCalls;
            return false;
        };
        if (APP::Setup(callback) || halCalls != 0 || injectionCalls != 1)
            return 1;
        std::puts("PASS missing_callback_stops_before_hal_locale_apps");
        std::puts("PASS false_callback_stops_before_hal_locale_apps");
        return 0;
    }
    if (std::strcmp(argv[1], "describe") == 0)
    {
        using namespace ASSET_POOL_LAYOUT;
        std::printf("{\"static_asset_size\":%zu,\"webpage_offset\":%zu,\"members\":[", kStaticAssetBytes, kWebPageOffset);
        for (std::size_t i = 0; i < kViewerMemberCount; ++i)
            std::printf("%s[%zu,%zu]", i == 0 ? "" : ",", kViewerMembers[i].offset, kViewerMembers[i].capacity);
        std::puts("]}");
        return 0;
    }
    if (std::strcmp(argv[1], "dump") == 0 && argc == 3)
    {
        std::unique_ptr<StaticAsset_t> asset(new StaticAsset_t());
        return AssetPool::DumpStaticAsset(argv[2], asset.get()) ? 0 : 1;
    }
    const bool generate = std::strcmp(argv[1], "generate") == 0;
    if (!generate && std::strcmp(argv[1], "load") != 0)
        return 2;
    bool missing = false;
    std::unique_ptr<StaticAsset_t> asset(generate ? AssetPool::CreateStaticAsset()
                                                  : AssetPool::GetStaticAssetFromBin(&missing));
    if (!asset)
    {
        std::printf("HOST_ASSETPOOL_FAILED missing=%u\n", missing ? 1U : 0U);
        return 1;
    }
    if (generate && (asset->Color.AppFile.background != 0xFFFFFF || asset->Color.AppWaveform.primary != 0xFF6161 ||
                     std::strcmp(asset->Text.TextEN.AppName_EduVolt, "VoltageMonitor") != 0))
        return 1;
    std::puts(generate ? "HOST_CONTAINER_GENERATED_DEFAULTS_PRESERVED" : "HOST_CONTAINER_LOADED");
    return 0;
}
