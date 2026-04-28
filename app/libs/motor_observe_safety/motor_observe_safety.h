/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <string>

namespace MOTOR_OBSERVE
{
    enum class SafetyState : uint8_t
    {
        SafeDisabled = 0,
        OutputArmed,
        OutputEnabled,
        Fault,
        FaultCleared,
    };

    class SafetyController
    {
    public:
        static constexpr int kMinTargetPercent = -100;
        static constexpr int kMaxTargetPercent = 100;

        SafetyController();

        void resetToSafeDisabled();
        bool prepareOutput();
        bool enableOutput();
        void disableOutput();
        void leaveMode();
        void timeout();
        void setFault(const std::string& reason);
        bool clearFault();

        void setTargetPercent(int targetPercent);
        int getTargetPercent() const;
        SafetyState getState() const;
        bool isPhysicalOutputAllowed() const;
        bool hasFault() const;
        const std::string& getFaultReason() const;

    private:
        SafetyState _state = SafetyState::SafeDisabled;
        int _targetPercent = 0;
        std::string _faultReason;

        static int _clampTargetPercent(int targetPercent);
    };
} // namespace MOTOR_OBSERVE
