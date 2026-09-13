/*
* SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
*
* SPDX-License-Identifier: MIT
*/
#pragma once
#include <mooncake.h>
#include <functional>
#include "assets/assets.h"
#include "hal/hal.h"

namespace APP
{
    struct SetupCallback_t
    {
        std::function<bool()> AssetPoolInjection = nullptr;
        std::function<void()> HalInjection = nullptr;
    };

    // False stops before HAL, locale/font access, or application startup.
    bool Setup(SetupCallback_t callback);
    void Loop();
    void Destroy();
} // namespace APP
