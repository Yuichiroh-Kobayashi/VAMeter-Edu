/*
 * SPDX-License-Identifier: MIT
 */
#include "motor_observe_backend.h"

namespace MOTOR_OBSERVE
{
    static int clampTargetPercent(int targetPercent)
    {
        if (targetPercent < SafetyController::kMinTargetPercent)
            return SafetyController::kMinTargetPercent;
        if (targetPercent > SafetyController::kMaxTargetPercent)
            return SafetyController::kMaxTargetPercent;
        return targetPercent;
    }

    bool NoopBackend::begin()
    {
        disarm();
        return true;
    }

    void NoopBackend::disarm()
    {
        _lastAppliedTargetPercent = 0;
        _faultReason.clear();
        _measurementPathRelayEnabled = false;
    }

    void NoopBackend::setMeasurementPathRelayEnabled(bool enabled) { _measurementPathRelayEnabled = enabled; }

    bool NoopBackend::isMeasurementPathRelayEnabled() const { return _measurementPathRelayEnabled; }

    void NoopBackend::setTargetPercent(int targetPercent)
    {
        _lastAppliedTargetPercent = clampTargetPercent(targetPercent);
    }

    void NoopBackend::update() {}

    bool NoopBackend::hasFault() const { return false; }

    const std::string& NoopBackend::getFaultReason() const { return _faultReason; }

    int NoopBackend::getLastAppliedTargetPercent() const { return _lastAppliedTargetPercent; }

    BackendController::BackendController(SafetyController& safetyController, Backend& backend)
        : _safetyController(safetyController), _backend(backend)
    {
    }

    bool BackendController::begin()
    {
        const bool begun = _backend.begin();
        _safetyController.resetToSafeDisabled();
        _backend.disarm();
        _syncMeasurementPathRelay();
        return begun;
    }

    bool BackendController::prepareOutput()
    {
        _applyZeroTarget();
        const bool prepared = _safetyController.prepareOutput();
        _syncMeasurementPathRelay();
        return prepared;
    }

    bool BackendController::enableOutput()
    {
        const bool enabled = _safetyController.enableOutput();
        _syncMeasurementPathRelay();
        return enabled;
    }

    void BackendController::disableOutput()
    {
        _safetyController.disableOutput();
        _backend.disarm();
        _syncMeasurementPathRelay();
    }

    void BackendController::leaveMode()
    {
        _safetyController.leaveMode();
        _backend.disarm();
        _syncMeasurementPathRelay();
    }

    void BackendController::timeout()
    {
        _safetyController.timeout();
        _backend.disarm();
        _syncMeasurementPathRelay();
    }

    void BackendController::setFault(const std::string& reason)
    {
        _safetyController.setFault(reason);
        _backend.disarm();
        _syncMeasurementPathRelay();
    }

    bool BackendController::clearFault()
    {
        const bool cleared = _safetyController.clearFault();
        _backend.disarm();
        _syncMeasurementPathRelay();
        return cleared;
    }

    void BackendController::setTargetPercent(int targetPercent) { _safetyController.setTargetPercent(targetPercent); }

    void BackendController::update()
    {
        if (_backend.hasFault())
        {
            setFault(_backend.getFaultReason());
            return;
        }

        _syncMeasurementPathRelay();

        if (!_safetyController.isPhysicalOutputAllowed())
        {
            _applyZeroTarget();
            _backend.update();
            if (_backend.hasFault())
            {
                setFault(_backend.getFaultReason());
                return;
            }
            return;
        }

        _backend.setTargetPercent(_safetyController.getTargetPercent());
        _backend.update();
        if (_backend.hasFault())
        {
            setFault(_backend.getFaultReason());
            return;
        }
    }

    SafetyState BackendController::getState() const { return _safetyController.getState(); }

    int BackendController::getTargetPercent() const { return _safetyController.getTargetPercent(); }

    int BackendController::getLastAppliedTargetPercent() const { return _backend.getLastAppliedTargetPercent(); }

    bool BackendController::isPhysicalOutputAllowed() const { return _safetyController.isPhysicalOutputAllowed(); }

    bool BackendController::isMeasurementPathRelayEnabled() const { return _backend.isMeasurementPathRelayEnabled(); }

    bool BackendController::hasFault() const { return _safetyController.hasFault() || _backend.hasFault(); }

    const std::string& BackendController::getFaultReason() const
    {
        if (_safetyController.hasFault())
            return _safetyController.getFaultReason();

        return _backend.getFaultReason();
    }

    void BackendController::_applyZeroTarget() { _backend.setTargetPercent(0); }

    void BackendController::_syncMeasurementPathRelay()
    {
        const SafetyState state = _safetyController.getState();
        const bool enableRelay = (state == SafetyState::OutputArmed || state == SafetyState::OutputEnabled);
        _backend.setMeasurementPathRelayEnabled(enableRelay);
    }
} // namespace MOTOR_OBSERVE
