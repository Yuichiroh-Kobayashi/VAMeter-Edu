/*
* SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
*
* SPDX-License-Identifier: MIT
*/
#include "../app_settings.h"
#include "../../../hal/hal.h"
#include "../../utils/system/system.h"
#include "../../../assets/assets.h"
#include "../../app_startup_anim/app_startup_anim.h"
#include <string>
#include <vector>

using namespace SYSTEM::INPUTS;
using namespace SYSTEM::UI;
using namespace SmoothUIToolKit;
using namespace MOONCAKE::APPS;

// Page: /
void AppSettings::_on_page_root()
{
    spdlog::info("on page root");

    enum MenuIndex
    {
        kOperationGuide,
        kAbout,
        kDisplay,
        kBuzzer,
        kEncoder,
        kCalibration,
        kProbeMode,
        kFiles,
        kNetwork,
        kLanguage,
        kStartupImage,
        kBaseTest,
        kQuit,
    };

    int selected_index = 0;
    while (1)
    {
        std::vector<std::string> options;
        options.push_back("Operation Guide");                                   // case 0
        options.push_back(AssetPool::GetText().AppSettings_Option_About);       // case 1
        options.push_back(AssetPool::GetText().AppSettings_Option_Display);     // case 2
        options.push_back(AssetPool::GetText().AppSettings_Option_Buzzer);      // case 3
        options.push_back(AssetPool::GetText().AppSettings_Option_Encoder);     // case 4
        options.push_back(AssetPool::GetText().AppSettings_Option_Calibration); // case 5
        options.push_back("Probe Mode");       // case 6 (AssetPool::GetText().AppSettings_Option_Probe)
        options.push_back(AssetPool::GetText().AppName_Files);                 // case 7
        options.push_back(AssetPool::GetText().AppSettings_Option_Network);     // case 8
        // options.push_back(AssetPool::GetText().AppSettings_Option_OTA);
        options.push_back(AssetPool::GetText().AppSettings_Option_Language);    // case 9
        options.push_back(AssetPool::GetText().AppSettings_Option_StartupImage);// case 10
        options.push_back("Base Test");                                         // case 11
        // options.push_back("Factory Reset");
        options.push_back(AssetPool::GetText().AppSettings_Option_Quit);        // case 12

        selected_index = SelectMenuPage::CreateAndWaitResult(
            AssetPool::GetText().AppName_Settings, options, selected_index, &_data.select_page_theme);

        if (selected_index == -1)
            break;
        else if (selected_index == kQuit)
            break;

        else if (selected_index == kOperationGuide)
            AppStartupAnim::PopUpGuideMap(true);

        else if (selected_index == kAbout)
            _on_page_about();

        else if (selected_index == kDisplay)
            _on_page_display();

        else if (selected_index == kBuzzer)
            _on_page_buzzer();

        else if (selected_index == kEncoder)
            _on_page_encoder();

        else if (selected_index == kCalibration)
            _on_page_calibration();

        else if (selected_index == kProbeMode)
            _on_page_probe_mode();
            // _on_page_network();

        else if (selected_index == kFiles)
        {
            spdlog::info("opening Files from Settings");

            MOONCAKE::APP_PACKER_BASE* files_packer = nullptr;
            for (const auto& app_packer : mcAppGetFramework()->getInstalledAppList())
            {
                if (std::string(app_packer->getAppName()) == AssetPool::GetText().AppName_Files)
                {
                    files_packer = app_packer;
                    break;
                }
            }

            if (files_packer == nullptr)
            {
                spdlog::error("Files app packer not found");
                continue;
            }

            if (!mcAppGetFramework()->createAndStartApp(files_packer))
            {
                spdlog::error("failed to start Files app");
                continue;
            }

            return;
        }

        else if (selected_index == kNetwork)
            _on_page_network();
            // _on_page_ota_upgrade();

        else if (selected_index == kLanguage)
            _on_page_language();

        else if (selected_index == kStartupImage)
            _on_page_startup_image();

        else if (selected_index == kBaseTest)
            _on_page_base_test();

        //else if (selected_index == 11)
        //    _on_factory_reset();
    }

    // // Save changes
    // if (_data.is_changed)
    // {
    //     if (CreateConfirmPage(AssetPool::GetText().AppSettings_Notice_Saving, true, &_data.select_page_theme))
    //         HAL::SaveSystemConfig();
    // }

    // if (_data.need_reboot)
    // {
    //     if (CreateConfirmPage(AssetPool::GetText().Misc_RebootNow, true, &_data.select_page_theme))
    //         HAL::Reboot();
    // }
}
