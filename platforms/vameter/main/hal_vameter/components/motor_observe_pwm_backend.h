/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <string>

#include "libs/motor_observe_backend/motor_observe_backend.h"

namespace MOTOR_OBSERVE
{
    class VAMeterMotorObservePwmBackend : public Backend
    {
    public:
        bool begin() override;
        void disarm() override;
        void setMeasurementPathRelayEnabled(bool enabled) override;
        bool isMeasurementPathRelayEnabled() const override;
        void setTargetPercent(int targetPercent) override;
        void update() override;
        bool hasFault() const override;
        const std::string& getFaultReason() const override;
        int getLastAppliedTargetPercent() const override;

    private:
        bool _begun = false;
        bool _forwardChannelConfigured = false;
        bool _reverseChannelConfigured = false;
        bool _hasFault = false;
        bool _measurementPathRelayEnabled = false;
        int _pendingTargetPercent = 0;
        int _lastAppliedTargetPercent = 0;
        std::string _faultReason;

        static int _clampTargetPercent(int targetPercent);
        static uint32_t _targetPercentToDuty(int targetPercent);
        bool _applyDutyPercent(int forwardDutyPercent, int reverseDutyPercent);
        bool _tryApplyZeroDutyBestEffort();
        void _setFault(const char* operation, int errorCode);
    };
} // namespace MOTOR_OBSERVE
