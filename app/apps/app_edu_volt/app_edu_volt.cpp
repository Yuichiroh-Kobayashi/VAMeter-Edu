/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_edu_volt.h"
#include "../../hal/hal.h"
#include "../../assets/assets.h"
#include "../utils/system/system.h"
#include "spdlog/spdlog.h"
#include <cstdint>
#include <smooth_ui_toolkit.h>
#include <mooncake.h>
#include <memory>
#include "../app_power_monitor/app_power_monitor.h"

using namespace MOONCAKE::APPS;
using namespace SYSTEM::INPUTS;
using namespace SmoothUIToolKit;

const char* AppEduVolt_Packer::getAppName()
{
    const char* appName = AssetPool::GetText().AppName_EduVolt;
    if (appName == nullptr)
    {
        spdlog::error("App name is nullptr for AppEduVolt");
        return "UnknownApp"; // デフォルトの名前を返す
    }
    return appName;
}

void* AppEduVolt_Packer::getCustomData()
{
    auto* customData = AssetPool::GetStaticAsset();
    if (!customData)
    {
        spdlog::error("CustomData is nullptr for AppEduVolt");
        return nullptr;
    }
    return static_cast<void*>(&customData->Color.AppPowerMonitor.pageShuntVoltage);
}

void* AppEduVolt_Packer::getAppIcon()
{
    auto* icon = AssetPool::GetStaticAsset();
    if (!icon)
    {
        spdlog::error("App icon is nullptr for AppEduVolt");
        return nullptr;
    }
    return static_cast<void*>(icon->Image.AppEduVolt.app_icon);
}

// Like setup()...
void AppEduVolt::onResume()
{
    spdlog::info("{} onResume", getAppName());

    // PowerMonitorの電圧測定画面へ遷移 targetPageを（1）に設定
    MOONCAKE::Mooncake* framework = mcAppGetFramework();
    constexpr uint8_t kPageVolt = 1;
    auto* packer = new AppPower_monitor_Packer(kPageVolt);
    if (!framework->createAndStartApp(packer))
    {
        delete packer;
        spdlog::error("AppPower_monitorの起動に失敗しました。");
    }
    else
    {
        spdlog::info("Successfully started AppPower_monitor on page {}", kPageVolt);
    }

    destroyApp();
}

void AppEduVolt::onDestroy() { spdlog::info("{} onDestroy", getAppName()); }