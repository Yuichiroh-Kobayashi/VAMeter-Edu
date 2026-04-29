/*
 * SPDX-License-Identifier: MIT
 */
#include "app_motor_observe_bringup.h"

#include "../../assets/assets.h"
#include "../../hal/hal.h"
#include "../utils/system/system.h"
#include "../utils/system/ui/misc/misc.h"
#include "apps/utils/system/inputs/encoder/encoder.h"

#include <algorithm>
#include <cstdio>
#include <string>

#if defined(ESP_PLATFORM)
#include "hal_vameter/components/motor_observe_pwm_backend.h"
#endif

using namespace MOONCAKE::APPS;
using namespace SYSTEM::INPUTS;

namespace
{
    constexpr int kTargetStepPercent = 10;
    constexpr uint32_t kCsvSampleIntervalMs = MOTOR_OBSERVE::CSV::kDefaultSampleIntervalMs;

    std::unique_ptr<MOTOR_OBSERVE::Backend> createMotorObserveBackend()
    {
#if defined(ESP_PLATFORM)
        return std::unique_ptr<MOTOR_OBSERVE::Backend>(new MOTOR_OBSERVE::VAMeterMotorObservePwmBackend());
#else
        return std::unique_ptr<MOTOR_OBSERVE::Backend>(new MOTOR_OBSERVE::NoopBackend());
#endif
    }
} // namespace

void AppMotorObserveBringup::onResume()
{
    _data.backend = createMotorObserveBackend();
    _data.controller.reset(new MOTOR_OBSERVE::BackendController(_data.safetyController, *_data.backend));
    _data.backendReady = _data.controller->begin();
    _resetToSafeDisabled();
    _beginCsvSession(HAL::Millis());
    Encoder::Reset();
}

void AppMotorObserveBringup::onRunning()
{
    HAL::FeedTheDog();
    Button::Update();
    Encoder::Update();

    _syncFaultState();
    _handleInput();
    _syncFaultState();
    _logCsvIfDue();
    _render();
    HAL::CanvasUpdate();
    HAL::Delay(50);
}

void AppMotorObserveBringup::onDestroy()
{
    _resetToSafeDisabled();
    _stopCsvSession(HAL::Millis());

    _data.controller.reset();
    _data.backend.reset();
}

void AppMotorObserveBringup::_resetToSafeDisabled()
{
    _data.targetPercent = 0;
    if (_data.controller)
    {
        _data.controller->leaveMode();
        _data.controller->update();
    }
    _data.uiState = BringupState::SafeDisabled;
}

void AppMotorObserveBringup::_armOutput()
{
    if (!_data.controller || _data.uiState != BringupState::SafeDisabled)
        return;

    _data.targetPercent = 0;
    if (_data.controller->prepareOutput())
    {
        _data.controller->update();
        _data.uiState = BringupState::OutputArmed;
    }
}

void AppMotorObserveBringup::_enableOutput()
{
    if (!_data.controller || _data.uiState != BringupState::OutputArmed)
        return;

    _data.targetPercent = 0;
    if (_data.controller->enableOutput())
    {
        _data.controller->setTargetPercent(0);
        _data.controller->update();
        _data.uiState = BringupState::OutputEnabled;
    }
}

void AppMotorObserveBringup::_brakeStop()
{
    if (!_data.controller)
        return;

    _data.targetPercent = 0;
    _data.controller->setTargetPercent(0);
    _data.controller->update();
    _data.controller->disableOutput();
    _data.controller->update();
    _data.uiState = BringupState::SafeDisabled;
}

void AppMotorObserveBringup::_clearFault()
{
    if (!_data.controller || _data.uiState != BringupState::Fault)
        return;

    _data.targetPercent = 0;
    _data.controller->clearFault();
    _data.controller->update();
    _data.uiState = BringupState::SafeDisabled;
}

void AppMotorObserveBringup::_applyTargetPercent(int targetPercent)
{
    if (!_data.controller || _data.uiState != BringupState::OutputEnabled)
        return;

    _data.targetPercent =
        std::max(MOTOR_OBSERVE::SafetyController::kMinTargetPercent,
                 std::min(MOTOR_OBSERVE::SafetyController::kMaxTargetPercent, targetPercent));
    _data.controller->setTargetPercent(_data.targetPercent);
    _data.controller->update();
}

void AppMotorObserveBringup::_syncFaultState()
{
    if (!_data.controller)
        return;

    if (_data.controller->hasFault())
    {
        _data.targetPercent = 0;
        _data.controller->setTargetPercent(0);
        _data.controller->update();
        _data.uiState = BringupState::Fault;
    }
}

void AppMotorObserveBringup::_handleInput()
{
    if (!_data.controller)
        return;

    if (Button::Side()->wasClicked())
    {
        _openCsvDownloadQr();
        return;
    }

    if (Button::Encoder()->wasHold())
    {
        if (_data.uiState == BringupState::SafeDisabled)
            _armOutput();
        return;
    }

    if (Button::Encoder()->wasClicked())
    {
        if (_data.uiState == BringupState::OutputArmed)
            _enableOutput();
        else if (_data.uiState == BringupState::OutputEnabled)
            _brakeStop();
        else if (_data.uiState == BringupState::Fault)
            _clearFault();
        return;
    }

    if (_data.uiState != BringupState::OutputEnabled || !Encoder::WasMoved())
        return;

    const int direction = Encoder::GetDirection();
    if (direction > 0)
        _applyTargetPercent(_data.targetPercent + kTargetStepPercent);
    else if (direction < 0)
        _applyTargetPercent(_data.targetPercent - kTargetStepPercent);
}

void AppMotorObserveBringup::_beginCsvSession(uint32_t nowMs)
{
    _data.csvStartMs = nowMs;
    _data.lastCsvSampleMs = nowMs;
    _data.hasLastCsvSample = false;
    _data.lastCsvSafetyState = MOTOR_OBSERVE::SafetyState::SafeDisabled;
    _data.lastCsvRequestedTargetPercent = 0;
    _data.lastCsvAppliedTargetPercent = 0;
    _data.csvReady = _data.csvLogger.begin(_createCsvRow("start", nowMs));
}

void AppMotorObserveBringup::_stopCsvSession(uint32_t nowMs)
{
    if (!_data.csvLogger.isOpen())
    {
        _data.csvReady = false;
        return;
    }

    _data.csvLogger.stop(_createCsvRow("stop", nowMs));
    _data.csvReady = false;
}

void AppMotorObserveBringup::_openCsvDownloadQr()
{
    if (!_data.csvReady || !_data.csvLogger.isOpen())
        return;

    const std::string recordName = _data.csvLogger.fileName();

    _resetToSafeDisabled();
    _stopCsvSession(HAL::Millis());

    SYSTEM::UI::CreateDownloadQRPage(recordName);

    _resetToSafeDisabled();
    _beginCsvSession(HAL::Millis());
}

void AppMotorObserveBringup::_logCsvIfDue()
{
    if (!_data.csvReady || !_data.csvLogger.isOpen())
        return;

    const uint32_t nowMs = HAL::Millis();
    if (nowMs - _data.lastCsvSampleMs < kCsvSampleIntervalMs)
        return;

    _data.lastCsvSampleMs = nowMs;
    _data.csvLogger.append(_createCsvRow(_detectCsvEvent(), nowMs));
}

MOTOR_OBSERVE::CSV::Row AppMotorObserveBringup::_createCsvRow(const char* event, uint32_t nowMs)
{
    MOTOR_OBSERVE::CSV::Row row;
    row.elapsedMs = nowMs - _data.csvStartMs;
    row.event = event;
    row.note = "measurement_path_unknown_not_for_classroom_use";

    if (_data.controller)
    {
        row.safetyState = _data.controller->getState();
        row.physicalOutputAllowed = _data.controller->isPhysicalOutputAllowed();
        row.requestedTargetPercent = _data.targetPercent;
        row.appliedTargetPercent = _data.controller->getLastAppliedTargetPercent();
    }

#if defined(ESP_PLATFORM)
    row.physicalBackendAvailable = true;
#else
    row.physicalBackendAvailable = false;
#endif

    HAL::UpdatePowerMonitor();
    const auto& pmData = HAL::GetPowerMonitorData();
    row.measurement.voltageV = pmData.busVoltage;
    row.measurement.currentA = pmData.shuntCurrent;
    row.measurement.powerW = pmData.busPower;
    row.measurementPathCode = "unknown";
    row.voltageSemantics = "unknown";
    row.currentSemantics = "unknown";
    row.powerSemantics = "unknown";
    row.powerSource = "hal";
    row.pwmWaveformCaptured = false;
    row.abnormalFlag = _data.controller && _data.controller->hasFault();

    return row;
}

const char* AppMotorObserveBringup::_detectCsvEvent()
{
    if (!_data.controller)
        return "sample";

    const MOTOR_OBSERVE::SafetyState currentState = _data.controller->getState();
    const int currentRequestedTargetPercent = _data.targetPercent;
    const int currentAppliedTargetPercent = _data.controller->getLastAppliedTargetPercent();

    const char* event = "sample";
    if (_data.controller->hasFault())
        event = "fault";
    else if (!_data.hasLastCsvSample || currentState != _data.lastCsvSafetyState)
        event = "state_change";
    else if (currentRequestedTargetPercent != _data.lastCsvRequestedTargetPercent ||
             currentAppliedTargetPercent != _data.lastCsvAppliedTargetPercent)
        event = "target_change";

    _data.hasLastCsvSample = true;
    _data.lastCsvSafetyState = currentState;
    _data.lastCsvRequestedTargetPercent = currentRequestedTargetPercent;
    _data.lastCsvAppliedTargetPercent = currentAppliedTargetPercent;

    return event;
}

void AppMotorObserveBringup::_render()
{
    auto* canvas = HAL::GetCanvas();
    canvas->fillScreen(TFT_BLACK);
    canvas->setTextDatum(top_left);
    canvas->setTextSize(1);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);

    int y = 4;
    auto drawLine = [&](const char* text) {
        canvas->drawString(text, 6, y);
        y += 15;
    };

    char lineBuffer[80];
    drawLine("Motor Observe Bring-up");
    canvas->setTextColor(TFT_YELLOW, TFT_BLACK);
    drawLine("BRING-UP ONLY / 授業用ではない");
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    drawLine("MAKER-DRIVE: NOT CONNECTED");
    drawLine("Motor: NOT CONNECTED");

    std::snprintf(lineBuffer, sizeof(lineBuffer), "State: %s", _stateText());
    drawLine(lineBuffer);
    std::snprintf(lineBuffer, sizeof(lineBuffer), "Target: %d %%", _data.targetPercent);
    drawLine(lineBuffer);
    std::snprintf(lineBuffer, sizeof(lineBuffer), "Output: %s", _outputText());
    drawLine(lineBuffer);

    drawLine("GPIO9: M1A candidate");
    drawLine("GPIO8: M1B candidate");
    drawLine("GPIO10: NOT USED");
    drawLine("Base relay: NOT USED");
    std::snprintf(lineBuffer,
                  sizeof(lineBuffer),
                  "CSV: %s",
                  _data.csvReady ? _data.csvLogger.fileName().c_str() : "OPEN FAIL");
    drawLine(lineBuffer);

    if (!_data.backendReady)
    {
        canvas->setTextColor(TFT_RED, TFT_BLACK);
        drawLine("Backend begin: FAIL");
    }

    if (_data.controller && _data.controller->hasFault())
    {
        canvas->setTextColor(TFT_RED, TFT_BLACK);
        std::string faultLine = "Fault: " + _data.controller->getFaultReason();
        canvas->drawString(faultLine.c_str(), 6, y);
    }
}

const char* AppMotorObserveBringup::_stateText() const
{
    switch (_data.uiState)
    {
    case BringupState::SafeDisabled:
        return "SafeDisabled";
    case BringupState::OutputArmed:
        return "OutputArmed";
    case BringupState::OutputEnabled:
        return "OutputEnabled";
    case BringupState::Fault:
        return "Fault";
    }
    return "Unknown";
}

const char* AppMotorObserveBringup::_outputText() const
{
    if (_data.uiState != BringupState::OutputEnabled || _data.targetPercent == 0)
        return "Low-Low";
    if (_data.targetPercent > 0)
        return "PWM-Low";
    return "Low-PWM";
}
