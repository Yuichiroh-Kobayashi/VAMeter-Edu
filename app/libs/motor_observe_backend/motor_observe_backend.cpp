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
    }

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
        return begun;
    }

    bool BackendController::prepareOutput()
    {
        _applyZeroTarget();
        return _safetyController.prepareOutput();
    }

    bool BackendController::enableOutput() { return _safetyController.enableOutput(); }

    void BackendController::disableOutput()
    {
        _safetyController.disableOutput();
        _backend.disarm();
    }

    void BackendController::leaveMode()
    {
        _safetyController.leaveMode();
        _backend.disarm();
    }

    void BackendController::timeout()
    {
        _safetyController.timeout();
        _backend.disarm();
    }

    void BackendController::setFault(const std::string& reason)
    {
        _safetyController.setFault(reason);
        _backend.disarm();
    }

    bool BackendController::clearFault()
    {
        const bool cleared = _safetyController.clearFault();
        _backend.disarm();
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

        if (!_safetyController.isPhysicalOutputAllowed())
        {
            _applyZeroTarget();
            _backend.update();
            return;
        }

        _backend.setTargetPercent(_safetyController.getTargetPercent());
        _backend.update();
    }

    SafetyState BackendController::getState() const { return _safetyController.getState(); }

    int BackendController::getTargetPercent() const { return _safetyController.getTargetPercent(); }

    int BackendController::getLastAppliedTargetPercent() const { return _backend.getLastAppliedTargetPercent(); }

    bool BackendController::isPhysicalOutputAllowed() const { return _safetyController.isPhysicalOutputAllowed(); }

    bool BackendController::hasFault() const { return _safetyController.hasFault() || _backend.hasFault(); }

    const std::string& BackendController::getFaultReason() const
    {
        if (_safetyController.hasFault())
            return _safetyController.getFaultReason();

        return _backend.getFaultReason();
    }

    void BackendController::_applyZeroTarget() { _backend.setTargetPercent(0); }
} // namespace MOTOR_OBSERVE
