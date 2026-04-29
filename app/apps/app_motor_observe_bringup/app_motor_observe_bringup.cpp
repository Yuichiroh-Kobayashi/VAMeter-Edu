/*
 * SPDX-License-Identifier: MIT
 */
#include "app_motor_observe_bringup.h"

#include "../../assets/assets.h"
#include "../../hal/hal.h"
#include "../utils/system/system.h"
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
    _render();
    HAL::CanvasUpdate();
    HAL::Delay(50);
}

void AppMotorObserveBringup::onDestroy()
{
    if (_data.controller)
        _data.controller->leaveMode();

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
    _data.uiState = BringupState::OutputArmed;
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
