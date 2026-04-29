/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <memory>
#include <mooncake.h>

#include "libs/motor_observe_backend/motor_observe_backend.h"

namespace MOONCAKE
{
    namespace APPS
    {
        class AppMotorObserveBringup : public APP_BASE
        {
        private:
            enum class BringupState
            {
                SafeDisabled,
                OutputArmed,
                OutputEnabled,
                Fault,
            };

            struct Data_t
            {
                MOTOR_OBSERVE::SafetyController safetyController;
                std::unique_ptr<MOTOR_OBSERVE::Backend> backend;
                std::unique_ptr<MOTOR_OBSERVE::BackendController> controller;
                BringupState uiState = BringupState::SafeDisabled;
                int targetPercent = 0;
                bool backendReady = false;
            };
            Data_t _data;

            void _resetToSafeDisabled();
            void _armOutput();
            void _enableOutput();
            void _brakeStop();
            void _clearFault();
            void _applyTargetPercent(int targetPercent);
            void _syncFaultState();
            void _handleInput();
            void _render();
            const char* _stateText() const;
            const char* _outputText() const;

        public:
            void onResume() override;
            void onRunning() override;
            void onDestroy() override;
        };

        class AppMotorObserveBringup_Packer : public APP_PACKER_BASE
        {
        public:
            const char* getAppName() override { return "Motor Observe Bring-up"; }
            void* getAppIcon() override { return nullptr; }
            void* newApp() override { return new AppMotorObserveBringup; }
            void deleteApp(void* app) override { delete static_cast<AppMotorObserveBringup*>(app); }
            void* getCustomData() override { return nullptr; }
        };
    } // namespace APPS
} // namespace MOONCAKE
