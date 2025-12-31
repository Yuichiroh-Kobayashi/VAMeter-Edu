/*
* SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
*
* SPDX-License-Identifier: MIT
*/
#include <app.h>
#include "hal_desktop/hal_desktop.hpp"

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
            AssetPool::InjectStaticAsset(from_bin);
        } else {
            // 無ければ作って注入し、同時に bin を出力（desktop/build/ に生成されます）
            auto asset = AssetPool::CreateStaticAsset();
            AssetPool::InjectStaticAsset(asset);
#if defined(LGFX_SDL)
            // 実行カレントディレクトリに書き出し（例：platforms/desktop/build）
            AssetPool::DumpStaticAsset("AssetPool-VAMeter.bin", asset);
#endif
        }
    };

    callback.HalInjection = []() { HAL::Inject(new HAL_Desktop(240, 240)); };

    APP::Setup(callback);
}

void loop() { APP::Loop(); }
