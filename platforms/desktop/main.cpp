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

    callback.AssetPoolInjection = []() -> bool {
        bool missing = false;
        auto asset = AssetPool::GetStaticAssetFromBin(&missing);
        if (asset == nullptr && missing)
            asset = AssetPool::CreateStaticAsset();
        // A present but invalid container must never be silently replaced.
        if (asset == nullptr)
            return false;
        if (!AssetPool::InjectStaticAsset(asset))
        {
            delete asset;
            return false;
        }
        return true;
    };

    callback.HalInjection = []() { HAL::Inject(new HAL_Desktop(240, 240)); };

    if (!APP::Setup(callback))
        std::exit(EXIT_FAILURE);
}

void loop() { APP::Loop(); }
