/*
* SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
*
* SPDX-License-Identifier: MIT
*/
#include <app.h>
#include "hal_desktop/hal_desktop.hpp"

#include <cstdlib>

void setup()
{
    APP::SetupCallback_t callback;

    callback.AssetPoolInjection = []() {
        /*
        AssetPool::InjectStaticAsset(AssetPool::CreateStaticAsset());
        // AssetPool::InjectStaticAsset(AssetPool::GetStaticAssetFromBin());
        */
        // デスクトップでは .bin があればそれを優先、無ければ内蔵スタブにフォールバック
        if (auto from_bin = AssetPool::GetStaticAssetFromBin()) {
            if (!AssetPool::InjectStaticAsset(from_bin))
                std::exit(EXIT_FAILURE);
        } else {
            // 無ければ作って注入し、同時に bin を出力（desktop/build/ に生成されます）
            auto asset = AssetPool::CreateStaticAsset();
            if (asset == nullptr || !AssetPool::InjectStaticAsset(asset))
                std::exit(EXIT_FAILURE);
#if defined(LGFX_SDL)
            // 実行カレントディレクトリに書き出し（例：platforms/desktop/build）
            if (!AssetPool::DumpStaticAsset("AssetPool-VAMeter.bin", asset))
                std::exit(EXIT_FAILURE);
#endif
        }
    };

    callback.HalInjection = []() { HAL::Inject(new HAL_Desktop(240, 240)); };

    APP::Setup(callback);
}

void loop() { APP::Loop(); }
