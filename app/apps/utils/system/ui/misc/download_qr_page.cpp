/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "misc.h"
#include "../../inputs/inputs.h"
#include "../../../../../assets/assets.h"
#include "../../../../../hal/hal.h"
#include "../../../qrcode/qrcode.h"
#include "hal/utils/lgfx_fx/lgfx_fx.h"
#include <mooncake.h>
#include <vector>
#include <string>

using namespace SYSTEM::INPUTS;

namespace SYSTEM
{
    namespace UI
    {
        void CreateDownloadQRPage(const std::string& recordName)
        {
            spdlog::info("Starting local download server for: {}", recordName);

            // Start download server
            HAL::StartDownloadServer(recordName);

            // Show preparing status (Debug & UX)
            HAL::GetCanvas()->fillScreen(AssetPool::GetColor().AppSettings.background);
            AssetPool::LoadFont24(HAL::GetCanvas());
            HAL::GetCanvas()->setTextDatum(middle_center);
            HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionText);
            HAL::GetCanvas()->drawString("Release Button...", 120, 120);
            HAL::CanvasUpdate();

            // Wait for button release (debounce)
            while (1)
            {
                Button::Update();
                if (!Button::Encoder()->isPressed() && !Button::Side()->isPressed())
                    break;
                HAL::Delay(50);
            }

            // Clear button event buffer
            Button::Update();
            Button::Encoder()->wasClicked();
            Button::Side()->wasHold();

            // Get local IP and create download URL
            std::string localIP = HAL::GetLocalIP();
            std::string downloadUrl = "http://" + localIP + "/download/" + recordName;
            spdlog::info("Download URL: {}", downloadUrl);

            // Generate QR code bitmap
            std::vector<std::vector<bool>> qrcode_bitmap;
            // Use fully qualified name to avoid lint errors/ambiguity
            QRCODE::GetQrcodeBitmap(qrcode_bitmap, downloadUrl.c_str());

            // Get SSID
            std::string ssid = HAL::GetApWifiSsid();

            // Main Loop
            bool running = true;
            while (running)
            {
                HAL::FeedTheDog();
                Button::Update();

                // Exit on Click
                if (Button::Encoder()->wasClicked())
                {
                    spdlog::info("Encoder clicked, exiting download page");
                    running = false;
                }
                // Exit on Side Hold (or Click)
                if (Button::Side()->wasClicked() || Button::Side()->wasHold())
                {
                    spdlog::info("Side button active, exiting download page");
                    running = false;
                }

                if (!running)
                    break;

                // Render
                HAL::GetCanvas()->fillScreen(AssetPool::GetColor().AppSettings.background);

                // Title
                // HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_24);
                // HAL::GetCanvas()->setTextDatum(top_center);
                // HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionText);
                // HAL::GetCanvas()->drawString("Download", 120, 5);

                // SSID (Color changed to match About page scheme -> Option Text or specific color?)
                // User requested: "app_settings/about to same color scheme"
                // About page usually uses `AssetPool::GetColor().AppSettings.optionText` for text?
                // Or maybe the Title color?
                // Let's check About page implementation later, but for now use optionText for consistency.
                HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_14);
                HAL::GetCanvas()->setTextDatum(top_center);
                HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionText);
                HAL::GetCanvas()->drawString(ssid.c_str(), 120, 5); // Moved to top

                // QR Code
                // User requested larger QR code and removed client count.
                // Previous size: 90. Max width is 240.
                // Let's try size 140 or 150.
                int qrSize = 150;
                QRCODE::RenderQRCodeBitmap(qrcode_bitmap,
                                           120 - qrSize / 2,
                                           30, // y position
                                           qrSize,
                                           AssetPool::GetColor().AppSettings.optionText,
                                           AssetPool::GetColor().AppSettings.background);

                // File name & IP
                HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_14);
                HAL::GetCanvas()->setTextDatum(middle_center);
                HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppSettings.optionText);
                HAL::GetCanvas()->drawString(recordName.c_str(), 120, 30 + qrSize + 15);

                HAL::GetCanvas()->setTextColor(TFT_WHITE);
                HAL::GetCanvas()->drawString(localIP.c_str(), 120, 30 + qrSize + 35);

                // Instruction
                HAL::GetCanvas()->setTextColor(TFT_DARKGREY);
                HAL::GetCanvas()->drawString("Click to finish", 120, 220);

                HAL::CanvasUpdate();
                HAL::Delay(50);
            }

            // Stop server when page closes
            HAL::StopDownloadServer();
            spdlog::info("Download server stopped");
        }
    } // namespace UI
} // namespace SYSTEM
