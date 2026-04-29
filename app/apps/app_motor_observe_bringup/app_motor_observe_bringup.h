/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <memory>
#include <mooncake.h>

#include "libs/motor_observe_backend/motor_observe_backend.h"
#include "libs/motor_observe_csv/motor_observe_csv_logger.h"

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

            enum class CsvStatus
            {
                Ready,
                OpenFail,
                WriteFail,
                CloseFail,
                Empty,
            };

            struct Data_t
            {
                MOTOR_OBSERVE::SafetyController safetyController;
                std::unique_ptr<MOTOR_OBSERVE::Backend> backend;
                std::unique_ptr<MOTOR_OBSERVE::BackendController> controller;
                MOTOR_OBSERVE::CSV::FileLogger csvLogger;
                BringupState uiState = BringupState::SafeDisabled;
                int targetPercent = 0;
                bool backendReady = false;
                bool csvReady = false;
                CsvStatus csvStatus = CsvStatus::OpenFail;
                bool hasLastCsvSample = false;
                MOTOR_OBSERVE::SafetyState lastCsvSafetyState = MOTOR_OBSERVE::SafetyState::SafeDisabled;
                int lastCsvRequestedTargetPercent = 0;
                int lastCsvAppliedTargetPercent = 0;
                uint32_t csvStartMs = 0;
                uint32_t lastCsvSampleMs = 0;
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
            void _beginCsvSession(uint32_t nowMs);
            bool _stopCsvSession(uint32_t nowMs);
            void _openCsvDownloadQr();
            void _logCsvIfDue();
            MOTOR_OBSERVE::CSV::Row _createCsvRow(const char* event, uint32_t nowMs);
            const char* _detectCsvEvent();
            void _render();
            const char* _stateText() const;
            const char* _outputText() const;
            const char* _csvStatusText() const;

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
