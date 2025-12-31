/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include <cstdint>
#include <mooncake.h>

namespace MOONCAKE
{
    namespace APPS
    {
        /**
         * @brief Template
         *
         */
        class AppEduVolt : public APP_BASE
        { // AppEduVoltクラス: APP_BASEクラスを継承
        private:
        public:
            void onResume() override;
            void onDestroy() override;
        };

        class AppEduVolt_Packer : public APP_PACKER_BASE
        {
            const char* getAppName() override;
            void* getCustomData() override;
            void* getAppIcon() override;
            void* newApp() override { return new AppEduVolt; }
            void deleteApp(void* app) override { delete static_cast<AppEduVolt*>(app); }
        };
    } // namespace APPS
} // namespace MOONCAKE
