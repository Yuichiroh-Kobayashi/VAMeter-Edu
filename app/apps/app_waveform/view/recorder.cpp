/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "lgfx/v1/misc/enum.hpp"
#include "view.h"
#include "../../../hal/hal.h"
#include "../triggers/triggers.h"
#include "../../utils/system/system.h"
#include "../../../assets/assets.h"
#include <cmath>
#include <mooncake.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <new>
#include <memory>
#include <utility>

using namespace SmoothUIToolKit;
using namespace VIEWS;
using namespace SYSTEM::INPUTS;
using namespace SYSTEM::UI;

WaveFormRecorder::~WaveFormRecorder()
{
    // HAL owns the trigger after successful creation. A timeout intentionally
    // leaves both recorder state and trigger alive until the task finishes.
    HAL::DestroyVaRecorder();
    if (_data.config_panel != nullptr)
        delete _data.config_panel;
}

void WaveFormRecorder::init()
{
    _data.config_panel = new ConfigPanel;
    _data.config_panel->init();

    _data.string_y_offset = AssetPool::IsLocaleEn() ? 2 : 3;
    Encoder::Reset();

    // Clear input residue
    Button::Update();
    Button::Encoder()->wasClicked();
    Button::Encoder()->wasHold();
    Button::Side()->wasClicked();
    Button::Side()->wasHold();
}

/* -------------------------------------------------------------------------- */
/*                                   Update                                   */
/* -------------------------------------------------------------------------- */
void WaveFormRecorder::update()
{
    // Update inputs
    Button::Update();
    Encoder::Update();

    const bool sideHeld = Button::Side()->wasHold();
    const bool sideClicked = Button::Side()->wasClicked();
    if (_data.state != state_saving && sideHeld)
        _data.want_to_quit = true;
    else if (_data.state != state_saving && sideClicked)
        _handle_help_request();

    switch (_data.state)
    {
    case state_idle:
        _update_state_idle();
        break;
    case state_waiting_trigger:
        _update_state_waiting_trigger();
        break;
    case state_recording:
        _update_state_recording();
        break;
    case state_saving:
        _update_state_saving();
        break;
    default:
        break;
    }

    if (Button::Encoder()->wasHold())
    {
        // Relay toggle omitted
    }

}

// Override to add y zoom with threshold line
void WaveFormRecorder::_update_input()
{
    Waveform::_update_pm_data();
    Waveform::_update_chart_x_zoom();

    // Get a scaled buffer to avoid redundant float operation
    if (IsTriggerModeHasThreshold(getConfig().trigger_mode) && !IsTriggerModeChannelV(getConfig().trigger_mode))
        _data.threshold_a_scaled_buffer = getConfig().threshold * Waveform::_get_pm_data_a_scale();

    // Update chary y zoom
    if (IsTriggerModeHasThreshold(getConfig().trigger_mode))
    {
        // V
        if (IsTriggerModeChannelV(getConfig().trigger_mode))
            Waveform::_update_chart_y_zoom_with_third_value(getConfig().threshold, 0);
        // A
        else
            Waveform::_update_chart_y_zoom_with_third_value(0, _data.threshold_a_scaled_buffer);
    }
    else
        Waveform::_update_chart_y_zoom();
}

void WaveFormRecorder::_update_chart()
{
    _update_input();
    Waveform::_update_transition();
}

void WaveFormRecorder::_update_state_idle()
{
    // Update waveform
    if (!_data.config_panel->isPoppedOut())
    {
        _update_chart();
    }

    _data.config_panel->update(HAL::Millis());

    // Start recording
    if (!_data.config_panel->isPoppedOut() && Button::Encoder()->wasClicked())
    {
        if (_handle_start_recording())
        {
            spdlog::info("jump to state_waiting_trigger");
            _data.state = state_waiting_trigger;
        }
        else
            _handle_recorder_error();
    }

    _handle_render();
}

void WaveFormRecorder::_update_state_waiting_trigger()
{
    _update_chart();
    _handle_render();

    if (HAL::GetVaRecorderError() != VA_RECORDER::error_none)
    {
        _handle_recorder_error();
        return;
    }

    if (HAL::IsVaRecorderRecording())
    {
        spdlog::info("jump to state_recording");
        _data.state = state_recording;
        _data.recording_start_time_count = HAL::Millis();
    }
}

void WaveFormRecorder::_update_state_recording()
{
    _update_chart();
    _handle_render();

    if (HAL::GetVaRecorderError() != VA_RECORDER::error_none)
    {
        _handle_recorder_error();
        return;
    }

    if (!HAL::IsVaRecorderRecording())
    {
        spdlog::info("jump to state_saving");
        _data.state = state_saving;

        // // Render tag
        // HAL::GetCanvas()->fillSmoothRoundRect(60, 95, 120, 50, 10, AssetPool::GetColor().AppFile.selector);

        // HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppFile.optionText, AssetPool::GetColor().AppFile.selector);
        // HAL::GetCanvas()->setTextDatum(middle_center);
        // AssetPool::LoadFont24(HAL::GetCanvas());
        // HAL::GetCanvas()->drawString(
        //     AssetPool::GetText().AppWaveform_Saving, HAL::GetCanvas()->width() / 2, HAL::GetCanvas()->height() / 2);

        HAL::GetCanvas()->fillRectAlpha(0, 100, 240, 40, 160, TFT_BLACK);
        HAL::GetCanvas()->setTextDatum(middle_center);
        HAL::GetCanvas()->setTextColor(TFT_WHITE);
        AssetPool::LoadFont24(HAL::GetCanvas());
        HAL::GetCanvas()->drawString(AssetPool::GetText().AppWaveform_Saving, 120, 120);

        HAL::CanvasUpdate();
    }
}

void WaveFormRecorder::_update_state_saving()
{
    if (HAL::GetVaRecorderError() != VA_RECORDER::error_none)
    {
        _handle_recorder_error();
        return;
    }
    if (!HAL::IsVaRecorderSaving())
    {
        _data.has_finished_recording = true;
        spdlog::info("jump to state_idle");
        _data.state = state_idle;
    }
}

/* -------------------------------------------------------------------------- */
/*                                   Render                                   */
/* -------------------------------------------------------------------------- */
void WaveFormRecorder::_handle_render()
{
    // Render waveform
    if (!_data.config_panel->isPoppedOut())
    {
        Waveform::_update_render(false, true, true);
        _render_threshold_line();
        _render_rec_state_label();
    }

    // Render config panel
    if (!_data.config_panel->isHidden())
    {
        _data.config_panel->render();
    }

    NotificationBubble::UpdateAndRender();
    HAL::CanvasUpdate();
}

static constexpr int _threshold_line_tag_margin_left = 90;

void WaveFormRecorder::_render_threshold_line()
{
    if (!IsTriggerModeHasThreshold(getConfig().trigger_mode))
        return;

    int threshold_line_chart_y = 0;

    // V
    if (IsTriggerModeChannelV(getConfig().trigger_mode))
    {
        // Get chart y
        threshold_line_chart_y = Waveform::_chart_props.chart_v.getChartPoint(0, getConfig().threshold).y;
        // Get label
        Waveform::_get_string_buffer() = HAL::GetUnitAdaptatedVoltage(getConfig().threshold).value;
        Waveform::_get_string_buffer() += HAL::GetUnitAdaptatedVoltage(getConfig().threshold).unit;
    }
    // A
    else
    {
        threshold_line_chart_y = Waveform::_chart_props.chart_a.getChartPoint(0, _data.threshold_a_scaled_buffer).y;
        Waveform::_get_string_buffer() = HAL::GetUnitAdaptatedCurrent(getConfig().threshold).value;
        Waveform::_get_string_buffer() += HAL::GetUnitAdaptatedCurrent(getConfig().threshold).unit;
    }

    // Render line
    for (int i = 0; i < 3; i++)
        HAL::GetCanvas()->drawFastHLine(
            0, threshold_line_chart_y + i, HAL::GetCanvas()->width(), AssetPool::GetColor().AppWaveform.colorThresholdLine);

    // Render tag
    HAL::GetCanvas()->fillSmoothRoundRect(_threshold_line_tag_margin_left,
                                          threshold_line_chart_y,
                                          70,
                                          18,
                                          3,
                                          AssetPool::GetColor().AppWaveform.colorThresholdLine);

    HAL::GetCanvas()->setTextDatum(top_center);
    HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_14);

    HAL::GetCanvas()->setTextColor(TFT_WHITE, AssetPool::GetColor().AppWaveform.colorThresholdLine);
    HAL::GetCanvas()->drawString(
        Waveform::_get_string_buffer().c_str(), _threshold_line_tag_margin_left + 37, threshold_line_chart_y + 2);
}

static constexpr int _state_label_mergin_top_waiting = 145;
static constexpr int _state_label_mergin_left_waiting = 3;
static constexpr int _state_label_mergin_top_recording = 145;
static constexpr int _state_label_mergin_left_recording = 3;

void WaveFormRecorder::_render_rec_state_label()
{
    // Waiting trigger
    if (_data.state == state_waiting_trigger)
    {
        HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_14);
        // AssetPool::LoadFont14(HAL::GetCanvas());

        HAL::GetCanvas()->setTextDatum(top_left);
        HAL::GetCanvas()->setTextColor(AssetPool::GetColor().AppFile.optionText,
                                       AssetPool::GetColor().AppWaveform.colorStateLabelWaiting);

        HAL::GetCanvas()->fillSmoothRoundRect(_state_label_mergin_left_waiting,
                                              _state_label_mergin_top_waiting,
                                              75,
                                              23,
                                              6,
                                              AssetPool::GetColor().AppWaveform.colorStateLabelWaiting);

        HAL::GetCanvas()->drawString("WAITING", _state_label_mergin_left_waiting + 5, _state_label_mergin_top_waiting + 4);
        // HAL::GetCanvas()->drawString(AssetPool::GetText().AppWaveform_Waitting,
        //                              _state_label_mergin_left_waiting + 5,
        //                              _state_label_mergin_top_waiting + 2);
    }

    else if (_data.state == state_recording)
    {
        HAL::GetCanvas()->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibolditalic_14);
        // AssetPool::LoadFont14(HAL::GetCanvas());

        HAL::GetCanvas()->setTextDatum(top_left);
        HAL::GetCanvas()->setTextColor(TFT_WHITE, AssetPool::GetColor().AppWaveform.colorStateLabelRecording);

        // Time
        int total_seconds = (HAL::Millis() - _data.recording_start_time_count) / 1000;
        int minutes = total_seconds / 60;
        int seconds = total_seconds % 60;
        Waveform::_get_string_buffer() = spdlog::fmt_lib::format("REC: {:02d}:{:02d}", minutes, seconds);

        // Panel
        auto panel_width = HAL::GetCanvas()->textWidth(Waveform::_get_string_buffer().c_str());
        panel_width += 10;
        HAL::GetCanvas()->fillSmoothRoundRect(_state_label_mergin_left_recording,
                                              _state_label_mergin_top_recording,
                                              panel_width,
                                              23,
                                              6,
                                              AssetPool::GetColor().AppWaveform.colorStateLabelRecording);

        // Label
        HAL::GetCanvas()->drawString(Waveform::_get_string_buffer().c_str(),
                                     _state_label_mergin_left_recording + 5,
                                     _state_label_mergin_top_recording + 4);
    }
}

/* -------------------------------------------------------------------------- */
/*                                  Recording                                 */
/* -------------------------------------------------------------------------- */
bool WaveFormRecorder::_handle_start_recording()
{
    static constexpr uint32_t kEducationalRecordTimeMs = 10000;

    if (!HAL::DestroyVaRecorder())
    {
        _data.destroy_already_timed_out = true;
        return false;
    }

    spdlog::info("create educational manual trigger");
    std::unique_ptr<VA_RECORDER::TriggerBase> trigger(new (std::nothrow) Trigger_Manual);
    if (!trigger)
        return false;
    trigger->setRecordTime(kEducationalRecordTimeMs);
    trigger->setChannelMode(_mode == 1 ? VA_RECORDER::channel_voltage
                                      : (_mode == 2 ? VA_RECORDER::channel_current : VA_RECORDER::channel_both));

    return HAL::CreatVaRecorder(std::move(trigger));
}

void WaveFormRecorder::_handle_help_request()
{
    // Dedicated integration point for the future HelpEvent transport. No
    // project-wide HelpEvent sink exists yet, so keep measurement and recorder
    // state untouched and make the request observable in logs.
    spdlog::info("waveform help requested (HelpEvent transport not installed)");
}

void WaveFormRecorder::_handle_recorder_error()
{
    const VA_RECORDER::Error_t error = HAL::GetVaRecorderError();
    (void)HAL::TerminateMeasurementSession(LIVE_SHARE_SAFETY::TerminationReason::MeasurementFault);
    if (!_data.destroy_already_timed_out)
        HAL::DestroyVaRecorder();
    _data.destroy_already_timed_out = false;
    _data.state = state_idle;
    if (error == VA_RECORDER::error_insufficient_space || error == VA_RECORDER::error_storage_info_failed)
        HAL::PopWarning(AssetPool::GetText().AppWaveform_Error_NoSpace);
    else
        HAL::PopWarning(AssetPool::GetText().AppWaveform_Error_Recording);

    Button::Update();
    Button::Encoder()->wasClicked();
    Button::Encoder()->wasHold();
    Button::Side()->wasClicked();
    Button::Side()->wasHold();
    Encoder::Reset();
}
