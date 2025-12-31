/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../app_settings.h"
#include "../../../hal/hal.h"
#include "../../../assets/assets.h"
#include "../../utils/system/system.h"
#include "../../utils/qrcode/qrcode.h"
#include "../../utils/easter_egg/easter_egg.h"
#include <string>
#include <vector>

using namespace SYSTEM::INPUTS;
using namespace SYSTEM::UI;
using namespace SmoothUIToolKit;
using namespace MOONCAKE::APPS;
using namespace QRCODE;

static void _render_page_a(const int& yOffset)
{
    if (yOffset == 0)
    {
        HAL::GetCanvas()->setTextSize(1);
        HAL::GetCanvas()->setTextDatum(top_left);
        HAL::GetCanvas()->setTextColor(AssetPool::GetStaticAsset()->Color.AppSettings.optionText);
        AssetPool::LoadFont16(HAL::GetCanvas());
        HAL::GetCanvas()->setCursor(0, 40);
    }

    // VAMeter-Edu version info
    HAL::GetCanvas()->printf(" [%s]:\n  Edu V1.0.0\n", AssetPool::GetText().Misc_Version);

    HAL::GetCanvas()->setTextDatum(top_left);
    HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibold_16);

    int avatar_y_offset = -6;

    // Developer info
    HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionText, AssetPool::GetColor().AppSettings.background);
    HAL::GetCanvas()->drawString("Developer", 20, 90 + yOffset + avatar_y_offset);
    HAL::GetCanvas()->drawString("Special Thanks", 20, 150 + yOffset + avatar_y_offset);

    HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionTextLight,
                                   AssetPool::GetColor().AppSettings.background);
    HAL::GetCanvas()->drawString("Yuichiroh-Kobayashi", 20, 111 + yOffset + avatar_y_offset);
    HAL::GetCanvas()->drawString("@M5Stack", 20, 171 + yOffset + avatar_y_offset);

    // Nav dots
    int current_one = 0;
    for (int i = 0; i < 3; i++)
    {
        if (current_one == i)
            HAL::GetCanvas()->fillSmoothCircle(104 + i * 16, yOffset + 227, 5, AssetPool::GetColor().AppSettings.optionText);
        else
            HAL::GetCanvas()->fillSmoothCircle(
                104 + i * 16, yOffset + 227, 3, AssetPool::GetColor().AppSettings.optionTextLight);
    }
}

static void _render_page_b()
{
    const char* text =
        "https://www.hackster.io/Yuichiroh-Kobayashi/vameter-edu-easy-tester-for-everyone-learning-electricity-9d06c6";
    std::vector<std::vector<bool>> qrcode_bitmap;
    GetQrcodeBitmap(qrcode_bitmap, text);

    RenderQRCodeBitmap(qrcode_bitmap,
                       120 - 140 / 2,
                       40,
                       140,
                       AssetPool::GetColor().AppSettings.optionText,
                       AssetPool::GetColor().AppSettings.background);

    HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_14);
    HAL::GetCanvas()->setTextDatum(middle_center);
    HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionTextLight);
    HAL::GetCanvas()->drawString("----   DOCUMENT   ----", 120, 200);

    // Nav dots
    int current_one = 1;
    for (int i = 0; i < 3; i++)
    {
        if (current_one == i)
            HAL::GetCanvas()->fillSmoothCircle(104 + i * 16, 227, 5, AssetPool::GetColor().AppSettings.optionText);
        else
            HAL::GetCanvas()->fillSmoothCircle(104 + i * 16, 227, 3, AssetPool::GetColor().AppSettings.optionTextLight);
    }
}

static void _render_page_c()
{
    const char* text = "https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu";
    std::vector<std::vector<bool>> qrcode_bitmap;
    GetQrcodeBitmap(qrcode_bitmap, text);

    RenderQRCodeBitmap(qrcode_bitmap,
                       120 - 140 / 2,
                       40,
                       140,
                       AssetPool::GetColor().AppSettings.optionText,
                       AssetPool::GetColor().AppSettings.background);

    HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_14);
    HAL::GetCanvas()->setTextDatum(middle_center);
    HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionTextLight);
    HAL::GetCanvas()->drawString("----   GITHUB   ----", 120, 200);

    // Nav dots
    int current_one = 2;
    for (int i = 0; i < 3; i++)
    {
        if (current_one == i)
            HAL::GetCanvas()->fillSmoothCircle(104 + i * 16, 227, 5, AssetPool::GetColor().AppSettings.optionText);
        else
            HAL::GetCanvas()->fillSmoothCircle(104 + i * 16, 227, 3, AssetPool::GetColor().AppSettings.optionTextLight);
    }
}

void AppSettings::_on_page_about()
{
    spdlog::info("on page about");

    Encoder::Reset();

    uint32_t ticker = 0;
    bool game = false;
    int curent_page_index = 0;
    CreateNoticePage(
        "About",
        [](Transition2D& transition) { _render_page_a(transition.getValue().y); },
        [&ticker, &game, &curent_page_index]()
        {
            HAL::FeedTheDog();
            HAL::Delay(50);

            Button::Update();
            if (Button::Side()->wasHold())
                return true;

            if (Button::Encoder()->wasPressed())
            {
                ticker = HAL::Millis();
            }
            else if (Button::Encoder()->isHolding())
            {
                if (HAL::Millis() - ticker > 2000)
                {
                    game = true;
                    return true;
                }
            }

            // Page nav
            Encoder::Update();
            if (Encoder::WasMoved())
            {
                if (Encoder::GetDirection() < 0)
                    curent_page_index--;
                else
                    curent_page_index++;

                if (curent_page_index < 0)
                    curent_page_index = 0;
                else if (curent_page_index > 2)
                    curent_page_index = 2;

                HAL::GetCanvas()->fillScreen(AssetPool::GetColor().AppSettings.background);
                // Titile
                HAL::GetCanvas()->setTextSize(1);
                HAL::GetCanvas()->setTextDatum(top_center);
                HAL::GetCanvas()->setTextColor(AssetPool::GetStaticAsset()->Color.AppSettings.optionText);
                // AssetPool::LoadFont24(HAL::GetCanvas());
                HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_24);

                HAL::GetCanvas()->fillRect(
                    0, 0, HAL::GetCanvas()->width(), 30, AssetPool::GetStaticAsset()->Color.AppSettings.selector);
                HAL::GetCanvas()->drawString("About", HAL::GetCanvas()->width() / 2, 0);

                if (curent_page_index == 0)
                    _render_page_a(0);
                else if (curent_page_index == 1)
                    _render_page_b();
                else if (curent_page_index == 2)
                    _render_page_c();

                HAL::CanvasUpdate();
            }

            return false;
        });

    if (game)
        EasterEgg::CreateRandomEasterEgg();
}
