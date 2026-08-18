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
#include <cstring>
#include <string>
#include <new>
#include <memory>
#include <utility>

using namespace SmoothUIToolKit;
using namespace VIEWS;
using namespace SYSTEM::INPUTS;
using namespace SYSTEM::UI;

namespace
{
    // Boundary translation only: LOCAL_FAULT stays decoupled from
    // app/hal/types.h, so the HAL-facing enum is mapped here.
    LOCAL_FAULT::RecorderErrorSource ClassifyErrorSource(VA_RECORDER::Error_t error)
    {
        switch (error)
        {
        case VA_RECORDER::error_storage_info_failed:
            return LOCAL_FAULT::RecorderErrorSource::StorageInfoFailed;
        case VA_RECORDER::error_insufficient_space:
            return LOCAL_FAULT::RecorderErrorSource::InsufficientSpace;
        case VA_RECORDER::error_temp_prepare_failed:
            return LOCAL_FAULT::RecorderErrorSource::TempPrepareFailed;
        case VA_RECORDER::error_open_chunk_failed:
            return LOCAL_FAULT::RecorderErrorSource::OpenChunkFailed;
        case VA_RECORDER::error_write_chunk_failed:
            return LOCAL_FAULT::RecorderErrorSource::WriteChunkFailed;
        case VA_RECORDER::error_open_final_failed:
            return LOCAL_FAULT::RecorderErrorSource::OpenFinalFailed;
        case VA_RECORDER::error_write_final_failed:
            return LOCAL_FAULT::RecorderErrorSource::WriteFinalFailed;
        case VA_RECORDER::error_close_failed:
            return LOCAL_FAULT::RecorderErrorSource::CloseFailed;
        case VA_RECORDER::error_allocation_failed:
            return LOCAL_FAULT::RecorderErrorSource::AllocationFailed;
        case VA_RECORDER::error_task_create_failed:
            return LOCAL_FAULT::RecorderErrorSource::TaskCreateFailed;
        case VA_RECORDER::error_none:
        default:
            return LOCAL_FAULT::RecorderErrorSource::Unknown;
        }
    }

    const char* FaultCauseText(LOCAL_FAULT::Cause cause)
    {
        return cause == LOCAL_FAULT::Cause::NoSpace ? AssetPool::GetText().AppWaveform_Error_NoSpace
                                                     : AssetPool::GetText().AppWaveform_Error_Recording;
    }
} // namespace

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
    Button::Update();
    Encoder::Update();

    LIVE_SHARE_CONTROLLER::InputSnapshot input;
    input.sideHeld = Button::Side()->wasHold();
    input.sideClicked = Button::Side()->wasClicked();
    input.encoderClicked = Button::Encoder()->wasClicked();
    input.encoderHeld = Button::Encoder()->wasHold();

    LIVE_SHARE_CONTROLLER::ForegroundAction action = LIVE_SHARE_CONTROLLER::ForegroundAction::None;
    if (input.sideHeld && _data.state != state_saving)
        action = LIVE_SHARE_CONTROLLER::ForegroundAction::Exit;
    else if (input.encoderClicked && _data.state == state_idle && recordingInputAvailable())
        action = LIVE_SHARE_CONTROLLER::ForegroundAction::StartRecording;
    _update_with_input(input, action, true, true);
}

void WaveFormRecorder::updateForeground(const LIVE_SHARE_CONTROLLER::InputSnapshot& input,
                                        LIVE_SHARE_CONTROLLER::ForegroundAction action,
                                        bool liveShareInactive)
{
    _update_with_input(input, action, liveShareInactive, false);
}

void WaveFormRecorder::_update_with_input(const LIVE_SHARE_CONTROLLER::InputSnapshot& input,
                                          LIVE_SHARE_CONTROLLER::ForegroundAction action,
                                          bool liveShareInactive,
                                          bool legacyHelpBehavior)
{
    // Fault input ownership: while already latched, no input this frame may
    // reach the common Exit/Help/RetryRecorderCleanup routing below. Only
    // the state_fault case in the switch (and its own ACK gate) may act.
    const bool entered_as_fault_latched = (_data.state == state_fault);

    if (!entered_as_fault_latched)
    {
        if (action == LIVE_SHARE_CONTROLLER::ForegroundAction::Exit && _data.state != state_saving)
            _data.want_to_quit = true;
        else if (legacyHelpBehavior && _data.state != state_saving && input.sideClicked)
            _handle_help_request();

        if (action == LIVE_SHARE_CONTROLLER::ForegroundAction::RetryRecorderCleanup)
            _retry_recorder_cleanup();
    }

    switch (_data.state)
    {
    case state_idle:
        _update_state_idle(action == LIVE_SHARE_CONTROLLER::ForegroundAction::StartRecording, liveShareInactive);
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
    case state_fault:
        _update_state_fault(input);
        break;
    default:
        break;
    }

    // Same-frame priority: a recorder fault detected while processing the
    // entry state above always wins over an ordinary Exit queued earlier in
    // this same frame. Never let want_to_quit carry through a frame that
    // enters (or remains in) state_fault.
    if (_data.state == state_fault)
        _data.want_to_quit = false;

    if (input.encoderHeld)
    {
        // Relay toggle omitted
    }
}

LIVE_SHARE_CONTROLLER::RecorderActivity WaveFormRecorder::activity() const
{
    using LIVE_SHARE_CONTROLLER::RecorderActivity;
    if (_data.state == state_fault)
        return RecorderActivity::FaultLatched;
    if (_data.destroy_already_timed_out || (_data.state == state_idle && HAL::IsVaRecorderExist()))
        return RecorderActivity::BusyCleanup;
    switch (_data.state)
    {
    case state_waiting_trigger:
        return RecorderActivity::WaitingTrigger;
    case state_recording:
        return RecorderActivity::Recording;
    case state_saving:
        return RecorderActivity::Saving;
    case state_idle:
    default:
        return RecorderActivity::Idle;
    }
}

bool WaveFormRecorder::recordingInputAvailable() const
{
    return _data.state == state_idle && _data.config_panel != nullptr && !_data.config_panel->isPoppedOut();
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

void WaveFormRecorder::_update_state_idle(bool startRecordingRequested, bool liveShareInactive)
{
    // Update waveform
    if (!_data.config_panel->isPoppedOut())
    {
        _update_chart();
    }

    _data.config_panel->update(HAL::Millis());

    // Start recording
    if (startRecordingRequested && liveShareInactive &&
        LIVE_SHARE_CONTROLLER::CanStartRecording(activity(), LIVE_SHARE_SESSION::State::Inactive) &&
        !_data.config_panel->isPoppedOut())
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

static constexpr int _fault_title_y = 10;
static constexpr int _fault_body_start_y = 92;
static constexpr int _fault_body_line_height = 22;
static constexpr int _fault_ack_y = 222;

void WaveFormRecorder::_render_fault_screen()
{
    LGFX_SpriteFx* canvas = HAL::GetCanvas();
    const uint32_t bg = AssetPool::GetColor().AppWaveform.colorStateLabelRecording;

    canvas->fillScreen(bg);
    canvas->setTextColor(TFT_WHITE, bg);

    canvas->setTextDatum(top_center);
    AssetPool::LoadFont24(canvas);
    canvas->drawString(AssetPool::GetText().AppWaveform_Fault_Title, 120, _fault_title_y);

    // LoadFont14 is Montserrat/English-only by design (no CJK glyphs); route
    // non-English Fault body text to the locale-aware 16px font instead so
    // jp/cn characters render from the embedded CJK subset rather than
    // falling back to missing-glyph placeholder boxes.
    if (AssetPool::IsLocaleEn())
        AssetPool::LoadFont14(canvas);
    else
        AssetPool::LoadFont16(canvas);
    canvas->setTextDatum(middle_center);

    int line_y = _fault_body_start_y;
    // Allocation-free '\n'-bounded line splitter: some existing localization
    // strings (e.g. FaultCauseText's No-space/Rec-error variants) still
    // encode multiple display lines in one key. Byte-scanning for '\n' is
    // UTF-8-safe: continuation bytes are always >= 0x80, so this never
    // splits inside a multi-byte character.
    static constexpr std::size_t kFaultLineBufferSize = 48;
    auto drawLine = [&](const char* text)
    {
        const char* cursor = text;
        while (cursor != nullptr)
        {
            const char* newline = std::strchr(cursor, '\n');
            std::size_t segment_len =
                newline != nullptr ? static_cast<std::size_t>(newline - cursor) : std::strlen(cursor);
            if (segment_len >= kFaultLineBufferSize)
                segment_len = kFaultLineBufferSize - 1;

            char line_buffer[kFaultLineBufferSize];
            std::memcpy(line_buffer, cursor, segment_len);
            line_buffer[segment_len] = '\0';

            canvas->drawString(line_buffer, 120, line_y);
            line_y += _fault_body_line_height;
            cursor = newline != nullptr ? newline + 1 : nullptr;
        }
    };

    const LOCAL_FAULT::Presentation presentation = LOCAL_FAULT::SelectPresentation(_data.fault);
    switch (presentation)
    {
    case LOCAL_FAULT::Presentation::OutputUnconfirmed:
        drawLine(AssetPool::GetText().AppWaveform_Fault_OutputUnconfirmed);
        drawLine(AssetPool::GetText().AppWaveform_Fault_PowerOff);
        break;
    case LOCAL_FAULT::Presentation::CleanupPending:
        drawLine(AssetPool::GetText().AppWaveform_Fault_CleanupPending);
        if (_data.fault.relayOffSoftwareConfirmed)
            drawLine(AssetPool::GetText().AppWaveform_Fault_OutputOff);
        drawLine(AssetPool::GetText().AppWaveform_Fault_PowerCycle);
        break;
    case LOCAL_FAULT::Presentation::Normal:
    default:
        drawLine(AssetPool::GetText().AppWaveform_Fault_StoppedForSafety);
        drawLine(AssetPool::GetText().AppWaveform_Fault_OutputOff);
        drawLine(FaultCauseText(_data.fault.cause));
        break;
    }

    if (_data.fault.acknowledgementAllowed)
        canvas->drawString(AssetPool::GetText().AppWaveform_Fault_Ack, 120, _fault_ack_y);

    HAL::CanvasUpdate();
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
    _data.destroy_already_timed_out = false;

    spdlog::info("create educational manual trigger");
    std::unique_ptr<VA_RECORDER::TriggerBase> trigger(new (std::nothrow) Trigger_Manual);
    if (!trigger)
        return false;
    trigger->setRecordTime(kEducationalRecordTimeMs);
    trigger->setChannelMode(_mode == 1 ? VA_RECORDER::channel_voltage
                                       : (_mode == 2 ? VA_RECORDER::channel_current : VA_RECORDER::channel_both));

    return HAL::CreatVaRecorder(std::move(trigger));
}

bool WaveFormRecorder::_retry_recorder_cleanup()
{
    if (activity() != LIVE_SHARE_CONTROLLER::RecorderActivity::BusyCleanup)
        return activity() == LIVE_SHARE_CONTROLLER::RecorderActivity::Idle;
    if (!HAL::DestroyVaRecorder())
    {
        _data.destroy_already_timed_out = true;
        return false;
    }
    _data.destroy_already_timed_out = false;
    return true;
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
    // 1. capture/classify the already-observed recorder/start error.
    const VA_RECORDER::Error_t error = HAL::GetVaRecorderError();
    const bool is_start_or_trigger_failure = (error == VA_RECORDER::error_none);

    // 2/3. Relay OFF remains the first external safety side effect inside
    // LIVE_SHARE_SAFETY::Execute; the result is now retained instead of
    // discarded via (void).
    const LIVE_SHARE_SAFETY::Result safety_result =
        HAL::TerminateMeasurementSession(LIVE_SHARE_SAFETY::TerminationReason::MeasurementFault);

    // 4. attempt existing recorder cleanup exactly as required.
    bool recorder_cleanup_pending_or_failed = _data.destroy_already_timed_out;
    if (!_data.destroy_already_timed_out && !HAL::DestroyVaRecorder())
    {
        _data.destroy_already_timed_out = true;
        recorder_cleanup_pending_or_failed = true;
    }

    // 5. populate the fixed local fault payload.
    const LOCAL_FAULT::Cause cause = is_start_or_trigger_failure
                                        ? LOCAL_FAULT::Cause::StartOrTriggerCreationFailed
                                        : LOCAL_FAULT::ClassifyRecorderError(ClassifyErrorSource(error));
    _data.fault = LOCAL_FAULT::BuildPayload(
        cause, safety_result.relayOffConfirmed, safety_result.hasCleanupDebt(), recorder_cleanup_pending_or_failed);

    // 6. enter state_fault (not state_idle).
    _data.state = state_fault;

    // 7. clear/arm input state so the triggering press cannot acknowledge
    // the fault; a fresh edge is required starting next frame.
    Button::Update();
    Button::Encoder()->wasClicked();
    Button::Encoder()->wasHold();
    Button::Side()->wasClicked();
    Button::Side()->wasHold();
    Encoder::Reset();

    // A press already in flight when the fault starts (e.g. the Encoder was
    // held through the transition) must be fully released at least once
    // before any click counts as a fresh ACK edge; its own release-click
    // must not be treated as fresh.
    _data.fault_ack_pending_release = Button::Encoder()->isPressed();

    // 8. return to normal per-frame state-machine ownership (caller).
}

void WaveFormRecorder::_update_state_fault(const LIVE_SHARE_CONTROLLER::InputSnapshot& input)
{
    _render_fault_screen();

    if (_data.fault_ack_pending_release)
    {
        // Consume the carried-over press without treating its release (or
        // any click reported for this frame) as an ACK edge.
        if (!Button::Encoder()->isPressed())
            _data.fault_ack_pending_release = false;
        return;
    }

    if (!LOCAL_FAULT::CanAcknowledge(_data.fault, input.encoderClicked))
        return;

    _data.state = state_idle;
    _data.fault = LOCAL_FAULT::Payload();
    _data.fault_ack_pending_release = false;

    // Clear residue again on the ack transition so the click that just
    // acknowledged cannot be replayed as a fresh state_idle input edge.
    Button::Update();
    Button::Encoder()->wasClicked();
    Button::Encoder()->wasHold();
    Button::Side()->wasClicked();
    Button::Side()->wasHold();
    Encoder::Reset();
}
