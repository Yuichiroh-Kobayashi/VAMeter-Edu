/*
* SPDX-FileCopyrightText: 2025 Yuichiroh-Kobayashi
*
* SPDX-License-Identifier: MIT
*/
#include "../app_settings.h"
#include "../../../hal/hal.h"
#include "../../utils/system/system.h"
#include "../../../assets/assets.h"
#include <string>
#include <vector>

using namespace SYSTEM::INPUTS;
using namespace SYSTEM::UI;
using namespace SmoothUIToolKit;
using namespace MOONCAKE::APPS;

// Page: /probe_mode
void AppSettings::_on_page_probe_mode()
{
    spdlog::info("on page probe_mode");
    spdlog::info("Initial probeMode: {}", HAL::GetSystemConfig().probeMode ? "Training" : "Normal");

    // false = Standalone (normal probe), true = Analog metor training probe
    auto history_mode = HAL::GetSystemConfig().probeMode;

    int selected_index = HAL::GetSystemConfig().probeMode ? 0 : 1;

    while (1)
    {
        // std::vector<std::string> options = {" - No", " - Yes", " - Back"};
        std::vector<std::string> options;
        options.push_back(AssetPool::GetText().AppSettings_Option_No);
        options.push_back(AssetPool::GetText().AppSettings_Option_Yes);
        options.push_back(AssetPool::GetText().AppSettings_Option_Back);

        selected_index = SelectMenuPage::CreateAndWaitResult(
            AssetPool::GetText().AppSettings_Option_Probe, options, selected_index, &_data.select_page_theme);

        if (selected_index == -1)
            break;
        else if (selected_index == options.size() - 1)
            break;

        else if (selected_index == 0)
        {
            HAL::GetSystemConfig().probeMode = false; // normal probe
            spdlog::info("normal probe");
        }
        else if (selected_index == 1)
        {
            HAL::GetSystemConfig().probeMode = true; // Analog metor training probe
            spdlog::info("Analog metor training probe");
        }

        break;
    }

    // Check save
    if (history_mode != HAL::GetSystemConfig().probeMode)
    {
        spdlog::info("Saving new probeMode: {}", HAL::GetSystemConfig().probeMode ? "Training" : "Normal");
        HAL::SaveSystemConfig();
        spdlog::info("Saved new probeMode: {}", HAL::GetSystemConfig().probeMode ? "Training" : "Normal");
    }
}
