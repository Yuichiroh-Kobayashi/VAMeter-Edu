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
        class AppEduCurrent : public APP_BASE
        { // AppEduCurrentクラス: APP_BASEクラスを継承
        private:
        public:
            void onResume() override;
            void onDestroy() override;
        };

        class AppEduCurrent_Packer : public APP_PACKER_BASE
        {
            const char* getAppName() override;
            void* getCustomData() override;
            void* getAppIcon() override;
            void* newApp() override { return new AppEduCurrent; }
            void deleteApp(void* app) override { delete static_cast<AppEduCurrent*>(app); }
        };
    } // namespace APPS
} // namespace MOONCAKE
