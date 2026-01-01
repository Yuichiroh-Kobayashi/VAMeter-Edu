/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../app_settings.h"
#include "../../../hal/hal.h"
#include "../../utils/system/system.h"
#include "../../utils/qrcode/qrcode.h"
#include "../../../assets/assets.h"
#include "spdlog/fmt/bundled/core.h"
#include <string>
#include <vector>

using namespace SYSTEM::INPUTS;
using namespace SYSTEM::UI;
using namespace SmoothUIToolKit;
using namespace MOONCAKE::APPS;
using namespace QRCODE;

void AppSettings::_on_page_network()
{
    spdlog::info("on page network");

    SelectMenuPage::Theme_t theme;
    theme.background = AssetPool::GetColor().AppSettings.background;
    theme.optionText = AssetPool::GetColor().AppSettings.optionText;
    theme.selector = AssetPool::GetColor().AppSettings.selector;

    while (1)
    {
        std::vector<std::string> options;
        options.push_back(" - Connection Info");
        options.push_back(" - Configure via Web");

        // AP Suffix
        int suffix = HAL::NvsGet(NVS_KEY_AP_SUFFIX);
        if (suffix <= 0 || suffix > 40)
            suffix = 0;
        std::string suffixKey = fmt::format(" - AP Suffix: {}", suffix == 0 ? "Default" : fmt::format("{:02d}", suffix));
        options.push_back(suffixKey);

        options.push_back(" - Back");

        int selected = SelectMenuPage::CreateAndWaitResult("Network", options, 0, &theme);

        if (selected == -1 || selected == 3) // Back
            break;

        // Connection Info
        if (selected == 0)
        {
            std::string string_buffer;
            CreateNoticePage("Notice",
                             [&](Transition2D& transition)
                             {
                                 string_buffer =
                                     spdlog::fmt_lib::format(" WiFi Config:\n  [SSID]:\n   {}\n  [Password]:\n   {}",
                                                             HAL::GetSystemConfig().wifiSsid,
                                                             HAL::GetSystemConfig().wifiPassword);
                                 HAL::GetCanvas()->print(string_buffer.c_str());
                                 HAL::GetCanvas()->drawCenterString("Click to Back", 120, 216 + transition.getValue().y);
                             });
        }
        // Configure via Web
        else if (selected == 1)
        {
            HAL::StartWebServer(OnLogPageRender);

            // Pop wifi code
            auto text = fmt::format("WIFI:T:nopass;S:{};;", HAL::GetApWifiSsid());
            std::vector<std::vector<bool>> qrcode_bitmap;
            GetQrcodeBitmap(qrcode_bitmap, text.c_str());

            CreateNoticePage(
                "WiFi Config",
                [&](Transition2D& transition)
                {
                    RenderQRCodeBitmap(qrcode_bitmap,
                                       120 - 120 / 2,
                                       50 + transition.getValue().y,
                                       120,
                                       AssetPool::GetColor().AppSettings.optionText,
                                       AssetPool::GetColor().AppSettings.background);

                    HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_14);
                    HAL::GetCanvas()->setTextDatum(middle_center);
                    HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionTextLight);
                    std::string string_buffer = fmt::format("----   {}   ----", HAL::GetApWifiSsid());
                    HAL::GetCanvas()->drawString(string_buffer.c_str(), 120, 185 + transition.getValue().y);

                    // Lable
                    AssetPool::LoadFont24(HAL::GetCanvas());
                    HAL::GetCanvas()->setTextDatum(middle_center);
                    HAL::GetCanvas()->setTextColor(AssetPool::GetStaticAsset()->Color.AppSettings.optionText);
                    HAL::GetCanvas()->drawString(AssetPool::GetText().Misc_Text_ConnectAp, 120, 215 + transition.getValue().y);
                },
                [&]()
                {
                    HAL::FeedTheDog();
                    HAL::Delay(50);
                    Button::Update();
                    if (Button::Encoder()->wasClicked())
                        return true;
                    // Check sta num
                    if (HAL::GetApStaNum() != 0)
                        return true;
                    return false;
                });

            // Pop url code
            text = HAL::GetSystemConfigUrl();
            GetQrcodeBitmap(qrcode_bitmap, text.c_str());

            CreateNoticePage(
                "WiFi Config",
                [&](Transition2D& transition)
                {
                    RenderQRCodeBitmap(qrcode_bitmap,
                                       120 - 120 / 2,
                                       40 + transition.getValue().y,
                                       120,
                                       AssetPool::GetColor().AppSettings.optionText,
                                       AssetPool::GetColor().AppSettings.background);

                    HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_14);
                    HAL::GetCanvas()->setTextDatum(middle_center);
                    HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionTextLight);
                    HAL::GetCanvas()->drawString(text.c_str(), 120, 180 + transition.getValue().y);

                    // Lable
                    AssetPool::LoadFont24(HAL::GetCanvas());
                    HAL::GetCanvas()->setTextDatum(middle_center);
                    HAL::GetCanvas()->setTextColor(AssetPool::GetStaticAsset()->Color.AppSettings.optionText);
                    HAL::GetCanvas()->drawString(AssetPool::GetText().Misc_Text_OpenLink, 120, 215 + transition.getValue().y);
                },
                nullptr);

            HAL::StopWebServer();
        }
        // AP Suffix Setting
        else if (selected == 2)
        {
            std::vector<std::string> suffixOptions;
            suffixOptions.push_back("00 (Default)");
            for (int i = 1; i <= 40; i++)
            {
                suffixOptions.push_back(fmt::format("{:02d}", i));
            }

            // Find current index
            int currentSuffix = HAL::NvsGet(NVS_KEY_AP_SUFFIX);
            if (currentSuffix < 0 || currentSuffix > 40)
                currentSuffix = 0;

            int newSuffix = SelectMenuPage::CreateAndWaitResult("AP Suffix", suffixOptions, currentSuffix, &theme);

            if (newSuffix != -1)
            {
                HAL::NvsSet(NVS_KEY_AP_SUFFIX, newSuffix);
            }
        }
    }
}
