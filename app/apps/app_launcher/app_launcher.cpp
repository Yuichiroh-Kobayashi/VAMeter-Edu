/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_launcher.h"
#include "../utils/system/system.h"
#include "../app_waveform/app_waveform.h"
#include "../../hal/hal.h"
#include "../../assets/assets.h"
#include "core/transition3d/transition3d.h"
#include "spdlog/spdlog.h"
#include <mooncake.h>

using namespace MOONCAKE::APPS;

using namespace SmoothUIToolKit;

void AppLauncher::onCreate()
{
    spdlog::info("{} onCreate", getAppName());
    startApp();
    setAllowBgRunning(true);
}

// static bool _is_just_boot_up = true; 画面記憶を削除

void AppLauncher::onResume()
{
    spdlog::info("{} onResume", getAppName());

    // If just boot up, open history app directly
    /* 画面記憶を削除
    if (_is_just_boot_up)
    {
        _is_just_boot_up = false;

        // Create app
        if (HAL::NvsGet(NVS_KEY_APP_HISTORY) == 5)
        {
            mcAppGetFramework()->createAndStartApp(mcAppGetFramework()->getInstalledAppList()[2]);
            VIEW::LauncherView::SetLastSelectedOptionIndex(1);
        }
        else
        {
            mcAppGetFramework()->createAndStartApp(mcAppGetFramework()->getInstalledAppList()[1]);
            VIEW::LauncherView::SetLastSelectedOptionIndex(0);
        }

        // Stack launcher into background
        closeApp();

        return;
    }
    */

    _create_launcher_view();
}

void AppLauncher::onRunning()
{
    // Guide screen
    if (_data.showing_guide)
    {
        HAL::GetCanvas()->pushImage(0, 0, 240, 240, (uint16_t*)_data.guide_image_asset);
        HAL::CanvasUpdate();

        // Wait for click
        SYSTEM::INPUTS::Button::Update();
        if (SYSTEM::INPUTS::Button::Encoder()->wasClicked())
        {
            // Turn Relay ON when entering measurement mode
            if (_data.menu_state == menu_main) // Current or Voltage selected
            {
                HAL::SetBaseRelay(true);
            }

            _data.showing_guide = false;

            // Advance state passed from callback
            if (_data.guide_image_asset == (void*)AssetPool::GetStaticAsset()->Image.AppEduCurrent.guide_current)
                _data.menu_state = menu_current;
            else if (_data.guide_image_asset == (void*)AssetPool::GetStaticAsset()->Image.AppEduVolt.guide_volt)
                _data.menu_state = menu_voltage;

            VIEW::LauncherView::SetLastSelectedOptionIndex(0);
            _create_launcher_view();
        }
        return;
    }

    _update_launcher_view();
}

void AppLauncher::onRunningBG()
{
    // If only launcher left
    if (mcAppGetFramework()->getAppManager()->getCreatedAppNum() <= 1)
    {
        // Back to the game
        startApp();
    }
}

void AppLauncher::onPause() { _destroy_launcher_view(); }

void AppLauncher::onDestroy() { spdlog::info("{} onDestroy", getAppName()); }

/* -------------------------------------------------------------------------- */
/*                                    View                                    */
/* -------------------------------------------------------------------------- */
void AppLauncher::_create_launcher_view()
{
    // Create menu
    _data.launcher_view = new VIEW::LauncherView;

    auto find_packer = [&](const char* name) -> MOONCAKE::APP_PACKER_BASE*
    {
        for (const auto& app_packer : mcAppGetFramework()->getInstalledAppList())
        {
            if (std::string(app_packer->getAppName()) == name)
                return app_packer;
        }
        return nullptr;
    };

    // Main menu
    if (_data.menu_state == menu_main)
    {
        // 1. Voltage
        auto* packer = find_packer(AssetPool::GetText().AppName_EduVolt);
        if (packer)
        {
            VIEW::LauncherView::AppOptionProps_t option;
            if (packer->getCustomData() != nullptr)
                option.themeColor = *(uint32_t*)packer->getCustomData();
            option.icon = packer->getAppIcon();
            option.name = packer->getAppName();
            _data.launcher_view->addAppOption(option);
        }

        // 2. Current
        packer = find_packer(AssetPool::GetText().AppName_EduCurrent);
        if (packer)
        {
            VIEW::LauncherView::AppOptionProps_t option;
            if (packer->getCustomData() != nullptr)
                option.themeColor = *(uint32_t*)packer->getCustomData();
            option.icon = packer->getAppIcon();
            option.name = packer->getAppName();
            _data.launcher_view->addAppOption(option);
        }

        // 3. USB-C
        packer = find_packer(AssetPool::GetText().AppName_PowerMonitor);
        if (packer)
        {
            VIEW::LauncherView::AppOptionProps_t option;
            if (packer->getCustomData() != nullptr)
                option.themeColor = *(uint32_t*)packer->getCustomData();
            option.icon = packer->getAppIcon();
            option.name = packer->getAppName();
            _data.launcher_view->addAppOption(option);
        }

        // 4. Settings
        packer = find_packer(AssetPool::GetText().AppName_Settings);
        if (packer)
        {
            VIEW::LauncherView::AppOptionProps_t option;
            if (packer->getCustomData() != nullptr)
                option.themeColor = *(uint32_t*)packer->getCustomData();
            option.icon = packer->getAppIcon();
            option.name = packer->getAppName();
            _data.launcher_view->addAppOption(option);
        }
    }
    // Sub menus
    else
    {
        // 1. Monitor/Training
        auto* packer = (_data.menu_state == menu_current) ? find_packer(AssetPool::GetText().AppName_EduCurrent)
                                                          : find_packer(AssetPool::GetText().AppName_EduVolt);
        // USB-C
        if (_data.menu_state == menu_usbc)
            packer = find_packer(AssetPool::GetText().AppName_PowerMonitor);

        if (packer)
        {
            VIEW::LauncherView::AppOptionProps_t option;
            if (packer->getCustomData() != nullptr)
                option.themeColor = *(uint32_t*)packer->getCustomData();
            option.icon = packer->getAppIcon();
            option.name = packer->getAppName();
            _data.launcher_view->addAppOption(option);
        }

        // 2. Waveform
        packer = find_packer(AssetPool::GetText().AppName_Waveform);
        if (packer)
        {
            VIEW::LauncherView::AppOptionProps_t option;
            if (packer->getCustomData() != nullptr)
                option.themeColor = *(uint32_t*)packer->getCustomData();
            option.icon = packer->getAppIcon();
            option.name = packer->getAppName();
            _data.launcher_view->addAppOption(option);
        }

        // Back Option Removed per user request
    }

    // App opened callback
    _data.launcher_view->setAppOpenCallback(
        [&](int selectedIndex)
        {
            spdlog::info("open app by packer index: {}", selectedIndex);

            // Helper to find packer (safe to use inside callback)
            auto find_packer = [&](const char* name) -> MOONCAKE::APP_PACKER_BASE*
            {
                for (const auto& app_packer : mcAppGetFramework()->getInstalledAppList())
                {
                    if (std::string(app_packer->getAppName()) == name)
                        return app_packer;
                }
                return nullptr;
            };

            // Main menu
            if (_data.menu_state == menu_main)
            {
                if (selectedIndex == 0)
                {
                    // Voltage -> Show Guide -> Submenu (logic in onRunning)
                    _data.showing_guide = true;
                    _data.guide_image_asset = (void*)AssetPool::GetStaticAsset()->Image.AppEduVolt.guide_volt;
                    _destroy_launcher_view();
                    return;
                }
                else if (selectedIndex == 1)
                {
                    // Current -> Show Guide -> Submenu (Transition handled in onRunning to wait for user input)
                    _data.showing_guide = true;
                    _data.guide_image_asset = (void*)AssetPool::GetStaticAsset()->Image.AppEduCurrent.guide_current;
                    _destroy_launcher_view();
                    return;
                }
                else if (selectedIndex == 2)
                {
                    _data.menu_state = menu_usbc;
                }
                else if (selectedIndex == 3)
                {
                    // Settings
                    mcAppGetFramework()->createAndStartApp(find_packer(AssetPool::GetText().AppName_Settings));
                    closeApp();
                    return;
                }

                // Refresh view
                _destroy_launcher_view();
                _create_launcher_view();
            }
            // Sub menus
            else
            {
                // Monitor/Training
                if (selectedIndex == 0)
                {
                    // Current, Voltage, or USB-C
                    // Launch original wrapper apps (or PowerMonitor for USB-C)
                    MOONCAKE::APP_PACKER_BASE* packer = nullptr;
                    if (_data.menu_state == menu_current)
                        packer = find_packer(AssetPool::GetText().AppName_EduCurrent);
                    else if (_data.menu_state == menu_voltage)
                        packer = find_packer(AssetPool::GetText().AppName_EduVolt);
                    else if (_data.menu_state == menu_usbc)
                        packer = find_packer(AssetPool::GetText().AppName_PowerMonitor);

                    if (packer)
                    {
                        mcAppGetFramework()->createAndStartApp(packer);
                        closeApp();
                    }
                    else
                    {
                        spdlog::error("App packer not found for state {}", (int)_data.menu_state);
                    }
                }
                // Waveform
                else if (selectedIndex == 1)
                {
                    if (_data.menu_state == menu_current)
                        AppWaveform::SetMode(AppWaveform::mode_current_only);
                    else if (_data.menu_state == menu_voltage)
                        AppWaveform::SetMode(AppWaveform::mode_volt_only);
                    else
                        AppWaveform::SetMode(AppWaveform::mode_both);

                    auto* packer = find_packer(AssetPool::GetText().AppName_Waveform);
                    if (packer)
                    {
                        mcAppGetFramework()->createAndStartApp(packer);
                        closeApp();
                    }
                    else
                    {
                        spdlog::error("Waveform app packer not found");
                    }
                }
            }
        });

    _data.launcher_view->init();
}

void AppLauncher::_update_launcher_view() { _data.launcher_view->update(HAL::Millis()); }

void AppLauncher::_destroy_launcher_view()
{
    if (_data.launcher_view != nullptr)
    {
        delete _data.launcher_view;
        _data.launcher_view = nullptr;
    }
}
