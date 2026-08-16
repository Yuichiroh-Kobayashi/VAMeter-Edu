/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view/view.h"
#include "view/live_share_view.h"
#include "../app_waveform/view/view.h"
#include "../../libs/live_share_session/live_share_session.h"
#include <cstdint>
#include <mooncake.h>

namespace MOONCAKE
{
    namespace APPS
    {
        /**
         * @brief Power_monitor
         *
         */
        class AppPower_monitor : public APP_BASE
        {
        private:
            struct Data_t
            {
                VIEWS::PmDataPage* view = nullptr;
                VIEWS::WaveFormRecorder* waveform_view = nullptr;
                VIEWS::LiveShareView* live_share_view = nullptr;
                LIVE_SHARE_SESSION::NetworkShareSession share_session;
                int current_page_num = 0;
                bool is_page_switched = false;
                bool is_usb_c_mode = false;
                int initial_page_num = 0;
                uint32_t pm_update_time_count = 0;
            };
            Data_t _data;
            void _update_view();
            void _check_page_switch();
            void _setup_page_bus_volt();
            void _setup_page_bus_power();
            void _setup_page_shunt_current();
            void _setup_page_shunt_volt();
            void _setup_page_simple_detail();
            void _setup_page_more_detail();
            void _setup_page_waveform();
            bool _is_educational_measurement() const;
            uint32_t _share_theme_color() const;
            void _update_share_workflow();
            void _execute_share_action(LIVE_SHARE_SESSION::Action action);
            void _finish_share_cleanup(WEB_SERVER_OWNER::StopResult result);

        public:
            // Constructor with initial page parameter
            AppPower_monitor(int initial_page = 0)
            {
                _data.view = nullptr;
                _data.current_page_num = initial_page;
                _data.is_page_switched = true;
            }
            // Delete copy constructor and assignment operator to prevent double-free
            AppPower_monitor(const AppPower_monitor&) = delete;
            AppPower_monitor& operator=(const AppPower_monitor&) = delete;
            void onResume() override;
            void onRunning() override;
            void onDestroy() override;
        };

        class AppPower_monitor_Packer : public APP_PACKER_BASE
        {
        private:
            uint8_t _initial_page; // Initial page number

        public:
            // Constructor with initial page parameter
            AppPower_monitor_Packer(int initial_page = 0) : _initial_page(initial_page) {}
            const char* getAppName() override;
            void* getAppIcon() override;
            // Create instance with initial page
            void* newApp() override { return new AppPower_monitor(_initial_page); }
            void deleteApp(void* app) override { delete static_cast<AppPower_monitor*>(app); }
            void* getCustomData() override;
        };
    } // namespace APPS
} // namespace MOONCAKE
