/*
 * SPDX-License-Identifier: MIT
 */
#include "motor_observe_safety.h"

namespace MOTOR_OBSERVE
{
    SafetyController::SafetyController() { resetToSafeDisabled(); }

    void SafetyController::resetToSafeDisabled()
    {
        _state = SafetyState::SafeDisabled;
        _targetPercent = 0;
        _faultReason.clear();
    }

    bool SafetyController::prepareOutput()
    {
        if (_state != SafetyState::SafeDisabled)
            return false;

        _targetPercent = 0;
        _state = SafetyState::OutputArmed;
        return true;
    }

    bool SafetyController::enableOutput()
    {
        if (_state != SafetyState::OutputArmed)
            return false;

        _state = SafetyState::OutputEnabled;
        return true;
    }

    void SafetyController::disableOutput() { resetToSafeDisabled(); }

    void SafetyController::leaveMode() { resetToSafeDisabled(); }

    void SafetyController::timeout() { resetToSafeDisabled(); }

    void SafetyController::setFault(const std::string& reason)
    {
        _targetPercent = 0;
        _faultReason = reason;
        _state = SafetyState::Fault;
    }

    bool SafetyController::clearFault()
    {
        if (_state != SafetyState::Fault)
            return false;

        // FaultCleared is intentionally transient; outputs remain disallowed and the controller returns to SafeDisabled.
        _state = SafetyState::FaultCleared;
        resetToSafeDisabled();
        return true;
    }

    void SafetyController::setTargetPercent(int targetPercent)
    {
        // Fault states must not retain a non-zero target for a later physical-output backend.
        if (_state == SafetyState::Fault || _state == SafetyState::FaultCleared)
        {
            _targetPercent = 0;
            return;
        }

        _targetPercent = _clampTargetPercent(targetPercent);
    }

    int SafetyController::getTargetPercent() const { return _targetPercent; }

    SafetyState SafetyController::getState() const { return _state; }

    bool SafetyController::isPhysicalOutputAllowed() const { return _state == SafetyState::OutputEnabled; }

    bool SafetyController::hasFault() const { return _state == SafetyState::Fault; }

    const std::string& SafetyController::getFaultReason() const { return _faultReason; }

    int SafetyController::_clampTargetPercent(int targetPercent)
    {
        if (targetPercent < kMinTargetPercent)
            return kMinTargetPercent;
        if (targetPercent > kMaxTargetPercent)
            return kMaxTargetPercent;
        return targetPercent;
    }
} // namespace MOTOR_OBSERVE
