/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view/view.h"
#include "../app_power_monitor/view/live_share_view.h"
#include "../../libs/live_share_controller/live_share_controller.h"
#include "../../libs/viewer_asset_contract/viewer_asset_contract.h"
#include <mooncake.h>

namespace MOONCAKE
{
    namespace APPS
    {
        /**
         * @brief Waveform
         *
         */
        class AppWaveform : public APP_BASE
        {
        public:
            enum WaveformMode_t
            {
                mode_both = 0,
                mode_volt_only,
                mode_current_only,
            };

        private:
            struct Data_t
            {
                VIEWS::WaveFormRecorder* view = nullptr;
                VIEWS::LiveShareView* live_share_view = nullptr;
                LIVE_SHARE_CONTROLLER::LiveShareController* live_share_controller = nullptr;
                VIEWER_ASSET_CONTRACT::DisplayProfile display_profile =
                    VIEWER_ASSET_CONTRACT::DisplayProfile::Invalid;
            };
            Data_t _data;
            static WaveformMode_t _mode;

        public:
            static void SetMode(WaveformMode_t mode) { _mode = mode; }
            static WaveformMode_t GetMode() { return _mode; }

            // Default constructor
            AppWaveform() = default;
            // Delete copy constructor and assignment operator to prevent double-free
            AppWaveform(const AppWaveform&) = delete;
            AppWaveform& operator=(const AppWaveform&) = delete;

            void onResume() override;
            void onRunning() override;
            void onDestroy() override;
            void _handle_recording_finished();
            void _sync_live_share_view();
            void _render_live_share_view();
        };

        class AppWaveform_Packer : public APP_PACKER_BASE
        {
            const char* getAppName() override;
            void* getAppIcon() override;
            void* newApp() override { return new AppWaveform; }
            void deleteApp(void* app) override { delete (AppWaveform*)app; }
            void* getCustomData() override;
        };
    } // namespace APPS
} // namespace MOONCAKE
