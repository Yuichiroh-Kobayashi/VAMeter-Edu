/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_waveform.h"
#include "../../hal/hal.h"
#include "../utils/system/system.h"
#include "../../assets/assets.h"
#include "../app_settings/app_settings.h"
#include "apps/utils/system/inputs/button/button.h"
#include "spdlog/spdlog.h"
#include <string>
#include <vector>

using namespace MOONCAKE::APPS;
using namespace SYSTEM::INPUTS;
using namespace SYSTEM::UI;

namespace
{
    WEB_SERVER_OWNER::StartResult StartSystemLive(void*) { return HAL::StartSystemLiveSharing(); }

    LIVE_SHARE_SESSION::TransportStopStatus BeginSystemLiveStop(void*) { return HAL::BeginSystemLiveStop(); }

    LIVE_SHARE_SESSION::TransportStopStatus PollSystemLiveStop(void*) { return HAL::PollSystemLiveStop(); }

    WEB_SERVER_OWNER::StopResult FinishSystemLiveStop(void*) { return HAL::FinishSystemLiveStop(); }

    std::uint32_t CurrentMillis(void*) { return HAL::Millis(); }

    std::uint8_t CurrentStationCount(void*) { return HAL::GetApStaNum(); }

    LIVE_SHARE_CONTROLLER::TransportCallbacks MakeLiveShareCallbacks()
    {
        LIVE_SHARE_CONTROLLER::TransportCallbacks callbacks;
        callbacks.startSystemLive = StartSystemLive;
        callbacks.beginSystemLiveStop = BeginSystemLiveStop;
        callbacks.pollSystemLiveStop = PollSystemLiveStop;
        callbacks.finishSystemLiveStop = FinishSystemLiveStop;
        callbacks.millis = CurrentMillis;
        callbacks.stationCount = CurrentStationCount;
        return callbacks;
    }
} // namespace

AppWaveform::WaveformMode_t AppWaveform::_mode = AppWaveform::mode_both;

const char* AppWaveform_Packer::getAppName() { return AssetPool::GetText().AppName_Waveform; }

// Theme color
void* AppWaveform_Packer::getCustomData() { return (void*)(&AssetPool::GetColor().AppWaveform.primary); }

// Icon
void* AppWaveform_Packer::getAppIcon() { return (void*)AssetPool::GetStaticAsset()->Image.AppWaveform.app_icon; }

// Like setup()...
void AppWaveform::onResume()
{
    spdlog::info("{} onResume", getAppName());
    _data.view = new VIEWS::WaveFormRecorder(AssetPool::GetColor().AppWaveform.primary, (int)_mode);
    _data.view->init();
    _data.live_share_controller = new LIVE_SHARE_CONTROLLER::LiveShareController(MakeLiveShareCallbacks());

    // Footprint
    HAL::NvsSet(NVS_KEY_APP_HISTORY, 5);
}

// Like loop()...
void AppWaveform::onRunning()
{
    Button::Update();
    Encoder::Update();

    LIVE_SHARE_CONTROLLER::InputSnapshot input;
    input.sideClicked = Button::Side()->wasClicked();
    input.sideHeld = Button::Side()->wasHold();
    input.encoderClicked = Button::Encoder()->wasClicked();
    input.encoderHeld = Button::Encoder()->wasHold();

    if (_data.live_share_controller->ownsInteraction())
    {
        _data.live_share_controller->update(input);
        _sync_live_share_view();
        if (_data.live_share_controller->ownsInteraction())
            _render_live_share_view();
        return;
    }

    const LIVE_SHARE_CONTROLLER::RecorderActivity recorderActivity = _data.view->activity();
    const LIVE_SHARE_CONTROLLER::ForegroundAction action = LIVE_SHARE_CONTROLLER::ResolveWaveformInput(
        input, recorderActivity, _data.live_share_controller->state(), _data.view->recordingInputAvailable());

    _data.view->updateForeground(input, action, _data.live_share_controller->isInactive());

    if (action == LIVE_SHARE_CONTROLLER::ForegroundAction::StartLiveView)
    {
        _data.live_share_controller->requestStart(_data.view->activity());
        spdlog::info("Live View start result: {}",
                     LIVE_SHARE_SESSION::StartOutcomeName(_data.live_share_controller->lastStartOutcome()));
        _sync_live_share_view();
        if (_data.live_share_controller->ownsInteraction())
            _render_live_share_view();
    }

    // Check kill signal
    if (_data.view->want2quit())
        destroyApp();

    // Check finish
    if (_data.view->hasFinishedRecording())
        _handle_recording_finished();
}

void AppWaveform::onDestroy()
{
    (void)HAL::TerminateMeasurementSession(LIVE_SHARE_SAFETY::TerminationReason::MeasurementExit);
    if (_data.live_share_controller)
        _data.live_share_controller->measurementTerminationObserved(LIVE_SHARE_SESSION::MeasurementTermination::Exit);
    spdlog::info("{} onDestroy", getAppName());
    delete _data.view;
    delete _data.live_share_view;
    delete _data.live_share_controller;
    NotificationBubble::Free();
}

void AppWaveform::_sync_live_share_view()
{
    if (_data.live_share_controller->ownsInteraction() && _data.live_share_view == nullptr)
        _data.live_share_view = new VIEWS::LiveShareView;
    else if (_data.live_share_controller->isInactive() && _data.live_share_view != nullptr)
    {
        delete _data.live_share_view;
        _data.live_share_view = nullptr;
    }
}

void AppWaveform::_render_live_share_view()
{
    if (_data.live_share_view == nullptr)
        return;
    _data.live_share_view->update(_data.live_share_controller->state(),
                                  AssetPool::GetColor().AppWaveform.primary,
                                  HAL::GetSystemLiveWifiSsid(),
                                  HAL::GetSystemLiveViewerUrl(),
                                  _data.live_share_controller->lastStartOutcome());
}

void AppWaveform::_handle_recording_finished()
{
    // Destroy view
    delete _data.view;

    // What to do with
    // std::vector<std::string> option_list = {" - Open", " - Network", " - Delete", " - Continue"};
    std::vector<std::string> option_list;
    option_list.push_back(AssetPool::GetText().AppFiles_Option_Open);
    option_list.push_back(AssetPool::GetText().AppSettings_Option_Network);
    option_list.push_back(AssetPool::GetText().AppFiles_Option_Delete);
    option_list.push_back(AssetPool::GetText().AppSettings_Option_Back);

    SelectMenuPage::Theme_t theme;
    theme.background = AssetPool::GetColor().AppFile.background;
    theme.optionText = AssetPool::GetColor().AppFile.optionText;
    theme.selector = AssetPool::GetColor().AppFile.selector;

    while (1)
    {
        auto selected_index = SelectMenuPage::CreateAndWaitResult(HAL::GetLatestVaRecordName().c_str(), option_list, 0, &theme);

        // Quit
        if (selected_index == -1)
            break;
        else if (selected_index == option_list.size() - 1)
            break;

        // Preview
        else if (selected_index == 0)
        {
            auto record = HAL::GetLatestVaRecord();
            VaRecordViewer::CreateAndWait(&record);
        }

        // Network
        else if (selected_index == 1)
        {
            AppSettings::_on_page_network();
        }

        // Delete
        else if (selected_index == 2)
        {
            if (CreateConfirmPage(AssetPool::GetText().AppFiles_Confirm_Delete, false))
            {
                HAL::DeleteVaRecord(HAL::GetLatestVaRecordName());
                break;
            }
        }
    }

    // Recreate view
    _data.view = new VIEWS::WaveFormRecorder(AssetPool::GetColor().AppWaveform.primary, (int)_mode);
    _data.view->init();
}
