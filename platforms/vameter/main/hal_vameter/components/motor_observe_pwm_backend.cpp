/*
 * SPDX-License-Identifier: MIT
 */
#include "motor_observe_pwm_backend.h"

#include "../hal_config.h"

#include <algorithm>
#include <cstdint>

#include "driver/ledc.h"
#include "esp_err.h"

namespace MOTOR_OBSERVE
{
    namespace
    {
        constexpr ledc_mode_t motorObservePwmSpeedMode = LEDC_LOW_SPEED_MODE;
        constexpr ledc_timer_t motorObservePwmTimer = LEDC_TIMER_2;
        constexpr ledc_timer_bit_t motorObservePwmResolutionBits = LEDC_TIMER_10_BIT;
        constexpr uint32_t motorObservePwmFrequencyHz = 1000;
        constexpr ledc_channel_t motorObserveForwardPwmChannel = LEDC_CHANNEL_2;
        constexpr ledc_channel_t motorObserveReversePwmChannel = LEDC_CHANNEL_3;
        constexpr int motorObserveForwardPwmPin = HAL_PIN_BASE_GROVE_IOA;
        constexpr int motorObserveReversePwmPin = HAL_PIN_BASE_GROVE_IOB;
        constexpr int motorObserveDutyMinPercent = 0;
        constexpr int motorObserveDutyMaxPercent = 100;
        constexpr uint32_t motorObservePwmMaxDuty = (1U << static_cast<int>(motorObservePwmResolutionBits)) - 1U;

        bool applyDutyToChannel(ledc_channel_t channel, uint32_t duty)
        {
            esp_err_t result = ledc_set_duty(motorObservePwmSpeedMode, channel, duty);
            if (result != ESP_OK)
                return false;

            result = ledc_update_duty(motorObservePwmSpeedMode, channel);
            return result == ESP_OK;
        }
    } // namespace

    bool VAMeterMotorObservePwmBackend::begin()
    {
        _begun = false;
        _forwardChannelConfigured = false;
        _reverseChannelConfigured = false;
        _hasFault = false;
        _faultReason.clear();
        _pendingTargetPercent = 0;
        _lastAppliedTargetPercent = 0;

        ledc_timer_config_t timerConfig = {};
        timerConfig.speed_mode = motorObservePwmSpeedMode;
        timerConfig.duty_resolution = motorObservePwmResolutionBits;
        timerConfig.timer_num = motorObservePwmTimer;
        timerConfig.freq_hz = motorObservePwmFrequencyHz;
        timerConfig.clk_cfg = LEDC_AUTO_CLK;

        esp_err_t result = ledc_timer_config(&timerConfig);
        if (result != ESP_OK)
        {
            _setFault("ledc_timer_config", result);
            return false;
        }

        ledc_channel_config_t forwardChannelConfig = {};
        forwardChannelConfig.gpio_num = motorObserveForwardPwmPin;
        forwardChannelConfig.speed_mode = motorObservePwmSpeedMode;
        forwardChannelConfig.channel = motorObserveForwardPwmChannel;
        forwardChannelConfig.intr_type = LEDC_INTR_DISABLE;
        forwardChannelConfig.timer_sel = motorObservePwmTimer;
        forwardChannelConfig.duty = 0;
        forwardChannelConfig.hpoint = 0;

        result = ledc_channel_config(&forwardChannelConfig);
        if (result != ESP_OK)
        {
            _setFault("ledc_channel_config forward", result);
            return false;
        }
        _forwardChannelConfigured = true;

        ledc_channel_config_t reverseChannelConfig = {};
        reverseChannelConfig.gpio_num = motorObserveReversePwmPin;
        reverseChannelConfig.speed_mode = motorObservePwmSpeedMode;
        reverseChannelConfig.channel = motorObserveReversePwmChannel;
        reverseChannelConfig.intr_type = LEDC_INTR_DISABLE;
        reverseChannelConfig.timer_sel = motorObservePwmTimer;
        reverseChannelConfig.duty = 0;
        reverseChannelConfig.hpoint = 0;

        result = ledc_channel_config(&reverseChannelConfig);
        if (result != ESP_OK)
        {
            _setFault("ledc_channel_config reverse", result);
            return false;
        }
        _reverseChannelConfigured = true;

        _begun = true;
        if (!_applyDutyPercent(0, 0))
            return false;

        return true;
    }

    void VAMeterMotorObservePwmBackend::disarm()
    {
        _pendingTargetPercent = 0;
        _lastAppliedTargetPercent = 0;
        _tryApplyZeroDutyBestEffort();
    }

    void VAMeterMotorObservePwmBackend::setTargetPercent(int targetPercent)
    {
        if (_hasFault)
        {
            _pendingTargetPercent = 0;
            return;
        }

        _pendingTargetPercent = _clampTargetPercent(targetPercent);
    }

    void VAMeterMotorObservePwmBackend::update()
    {
        if (!_begun)
            return;

        if (_hasFault)
        {
            _pendingTargetPercent = 0;
            _lastAppliedTargetPercent = 0;
            _tryApplyZeroDutyBestEffort();
            return;
        }

        // Initial dual-input policy: target 0 is Low/Low; never drive High/High, PWM/High, or High/PWM.
        int forwardDutyPercent = 0;
        int reverseDutyPercent = 0;

        if (_pendingTargetPercent > 0)
            forwardDutyPercent = _pendingTargetPercent;
        else if (_pendingTargetPercent < 0)
            reverseDutyPercent = -_pendingTargetPercent;

        if (_applyDutyPercent(forwardDutyPercent, reverseDutyPercent))
            _lastAppliedTargetPercent = _pendingTargetPercent;
    }

    bool VAMeterMotorObservePwmBackend::hasFault() const { return _hasFault; }

    const std::string& VAMeterMotorObservePwmBackend::getFaultReason() const { return _faultReason; }

    int VAMeterMotorObservePwmBackend::getLastAppliedTargetPercent() const { return _lastAppliedTargetPercent; }

    int VAMeterMotorObservePwmBackend::_clampTargetPercent(int targetPercent)
    {
        if (targetPercent < SafetyController::kMinTargetPercent)
            return SafetyController::kMinTargetPercent;
        if (targetPercent > SafetyController::kMaxTargetPercent)
            return SafetyController::kMaxTargetPercent;
        return targetPercent;
    }

    uint32_t VAMeterMotorObservePwmBackend::_targetPercentToDuty(int targetPercent)
    {
        const int dutyPercent = std::max(motorObserveDutyMinPercent, std::min(motorObserveDutyMaxPercent, targetPercent));
        return (motorObservePwmMaxDuty * static_cast<uint32_t>(dutyPercent)) / 100U;
    }

    bool VAMeterMotorObservePwmBackend::_applyDutyPercent(int forwardDutyPercent, int reverseDutyPercent)
    {
        forwardDutyPercent = std::max(motorObserveDutyMinPercent, std::min(motorObserveDutyMaxPercent, forwardDutyPercent));
        reverseDutyPercent = std::max(motorObserveDutyMinPercent, std::min(motorObserveDutyMaxPercent, reverseDutyPercent));

        if (forwardDutyPercent != 0 && reverseDutyPercent != 0)
        {
            _setFault("dual pwm duty", ESP_ERR_INVALID_ARG);
            return false;
        }

        if (!_tryApplyZeroDutyBestEffort())
        {
            _setFault("ledc_set_duty zero", ESP_FAIL);
            return false;
        }

        if (forwardDutyPercent != 0)
        {
            const uint32_t forwardDuty = _targetPercentToDuty(forwardDutyPercent);
            if (!applyDutyToChannel(motorObserveForwardPwmChannel, forwardDuty))
            {
                _setFault("ledc_set_duty forward", ESP_FAIL);
                return false;
            }
        }
        else if (reverseDutyPercent != 0)
        {
            const uint32_t reverseDuty = _targetPercentToDuty(reverseDutyPercent);
            if (!applyDutyToChannel(motorObserveReversePwmChannel, reverseDuty))
            {
                _setFault("ledc_set_duty reverse", ESP_FAIL);
                return false;
            }
        }

        return true;
    }

    bool VAMeterMotorObservePwmBackend::_tryApplyZeroDutyBestEffort()
    {
        bool ok = true;

        if (_forwardChannelConfigured)
            ok = applyDutyToChannel(motorObserveForwardPwmChannel, 0) && ok;

        if (_reverseChannelConfigured)
            ok = applyDutyToChannel(motorObserveReversePwmChannel, 0) && ok;

        return ok;
    }

    void VAMeterMotorObservePwmBackend::_setFault(const char* operation, int errorCode)
    {
        _pendingTargetPercent = 0;
        _lastAppliedTargetPercent = 0;
        _tryApplyZeroDutyBestEffort();
        _hasFault = true;
        _faultReason = std::string("Motor Observe PWM backend failed: ") + operation + " error=" + std::to_string(errorCode);
    }
} // namespace MOTOR_OBSERVE
