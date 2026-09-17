/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../hal_vameter.h"
#include "../hal_config.h"
#include <mooncake.h>
#include "libs/relay_interlock/relay_interlock.h"
#include "reverse_current_safety_device.h"
#include <freertos/FreeRTOS.h>

static bool _base_relay_state = false;
static RELAY_INTERLOCK::RelayInterlock _relay_interlock;
static portMUX_TYPE _relay_policy_mux = portMUX_INITIALIZER_UNLOCKED;

static void _set_base_relay_raw(bool state)
{
    _base_relay_state = state;
    gpio_set_level((gpio_num_t)HAL_PIN_BASE_RELAY_CTRL, _base_relay_state);
}

void HAL_VAMeter::_vabase_init()
{
    spdlog::info("vabase init");

    gpio_reset_pin((gpio_num_t)HAL_PIN_BASE_RELAY_CTRL);
    gpio_set_direction((gpio_num_t)HAL_PIN_BASE_RELAY_CTRL, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode((gpio_num_t)HAL_PIN_BASE_RELAY_CTRL, GPIO_PULLUP_PULLDOWN);

    portENTER_CRITICAL(&_relay_policy_mux);
    _set_base_relay_raw(false);
    _relay_interlock.initialize();
    portEXIT_CRITICAL(&_relay_policy_mux);
}

void HAL_VAMeter::setBaseRelay(bool state)
{
    portENTER_CRITICAL(&_relay_policy_mux);
    const RELAY_INTERLOCK::Command command = _relay_interlock.requestNormal(state);
    if (command.apply)
        _set_base_relay_raw(command.state);
    portEXIT_CRITICAL(&_relay_policy_mux);
}

bool HAL_VAMeter::getBaseRelayState()
{
    portENTER_CRITICAL(&_relay_policy_mux);
    const bool state = _base_relay_state;
    portEXIT_CRITICAL(&_relay_policy_mux);
    return state;
}

namespace REVERSE_CURRENT_SAFETY_DEVICE
{
    bool IsRelayPolicyInitialized()
    {
        portENTER_CRITICAL(&_relay_policy_mux);
        const bool initialized = _relay_interlock.isInitialized();
        portEXIT_CRITICAL(&_relay_policy_mux);
        return initialized;
    }

    void CommitFaultAndOpenRelay()
    {
        portENTER_CRITICAL(&_relay_policy_mux);
        const RELAY_INTERLOCK::Command command = _relay_interlock.commitFault();
        if (command.apply)
            _set_base_relay_raw(command.state);
        portEXIT_CRITICAL(&_relay_policy_mux);
    }
} // namespace REVERSE_CURRENT_SAFETY_DEVICE

void HAL_VAMeter::baseGroveStartTest()
{
    gpio_reset_pin((gpio_num_t)HAL_PIN_BASE_GROVE_IOA);
    gpio_set_direction((gpio_num_t)HAL_PIN_BASE_GROVE_IOA, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)HAL_PIN_BASE_GROVE_IOA, GPIO_PULLUP_ONLY);
    gpio_reset_pin((gpio_num_t)HAL_PIN_BASE_GROVE_IOB);
    gpio_set_direction((gpio_num_t)HAL_PIN_BASE_GROVE_IOB, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)HAL_PIN_BASE_GROVE_IOB, GPIO_PULLUP_ONLY);
}

void HAL_VAMeter::baseGroveStopTest()
{
    gpio_reset_pin((gpio_num_t)HAL_PIN_BASE_GROVE_IOA);
    gpio_reset_pin((gpio_num_t)HAL_PIN_BASE_GROVE_IOB);
}

bool HAL_VAMeter::baseGroveGetIoALevel() { return (bool)gpio_get_level((gpio_num_t)HAL_PIN_BASE_GROVE_IOA); }

bool HAL_VAMeter::baseGroveGetIoBLevel() { return (bool)gpio_get_level((gpio_num_t)HAL_PIN_BASE_GROVE_IOB); }
