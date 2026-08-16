/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_power_monitor.h"
#include "../../hal/hal.h"
#include "../utils/system/system.h"
#include "../../assets/assets.h"
#include "spdlog/spdlog.h"
#include "view/view.h"
#include <mooncake.h>
#include <string>

using namespace MOONCAKE::APPS;
using namespace SYSTEM::INPUTS;
using namespace SYSTEM::UI;
using namespace VIEWS;

const char* AppPower_monitor_Packer::getAppName() { return AssetPool::GetText().AppName_PowerMonitor; }

// Theme color
// void* AppPower_monitor_Packer::getCustomData() { return (void*)(&AssetPool::GetStaticAsset()->Color.AppPowerMonitor.primary);
// }
void* AppPower_monitor_Packer::getCustomData()
{
    auto app_history = HAL::NvsGet(NVS_KEY_APP_HISTORY);
    switch (app_history)
    {
    case 0:
        return static_cast<void*>(&AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageSimpleDetailBackground);
    case 1:
        return static_cast<void*>(&AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageBusVoltage);
    case 2:
        return static_cast<void*>(&AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageShuntCurrent);
    case 3:
        return static_cast<void*>(&AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageBusPower);
    case 4:
        return static_cast<void*>(&AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageMoreDetailBackground);
    case 5: // Waveform
        return static_cast<void*>(&AssetPool::GetStaticAsset()->Color.AppPowerMonitor.primary);
    default:
        break;
    }
    // spdlog::warn("no theme color on history {}", app_history);
    return static_cast<void*>(&AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageSimpleDetailBackground);
}

// Icon
void* AppPower_monitor_Packer::getAppIcon()
{
    return static_cast<void*>(AssetPool::GetStaticAsset()->Image.AppPowerMonitor.app_icon);
}

// Page 0~4
constexpr static int _total_page_num = 4;

// Like setup()...
void AppPower_monitor::onResume()
{
    spdlog::info("{} onResume", getAppName());

    _data.view = new PmDataPage;
    _data.is_page_switched = true;

    // Detect USB-C Mode (Starts at Page 0)
    if (_data.current_page_num == 0)
    {
        _data.is_usb_c_mode = true;
        spdlog::info("USB-C Mode Detected");
    }
    else
    {
        _data.is_usb_c_mode = false;
    }

    // 初期ページのセットアップ
    spdlog::info("Initial page number: {}", _data.current_page_num);
    // Store initial page to determine if we should lock navigation (Edu Mode)
    _data.initial_page_num = _data.current_page_num;

    /* Load history page
    _data.current_page_num = HAL::NvsGet(NVS_KEY_APP_HISTORY);
    if (_data.current_page_num > _total_page_num || _data.current_page_num < 0)
    {
        spdlog::error("wrong history page {}", _data.current_page_num);
        _data.current_page_num = 0;
    } */

    Encoder::Reset();
}

// Like loop()...
void AppPower_monitor::onRunning()
{
    _check_page_switch();
    if (_is_educational_measurement())
    {
        Button::Update();
        _update_share_workflow();
    }
    _update_view();
}

void AppPower_monitor::onDestroy()
{
    (void)HAL::TerminateMeasurementSession(LIVE_SHARE_SAFETY::TerminationReason::MeasurementExit);
    _data.share_session.measurementTerminationObserved(LIVE_SHARE_SESSION::MeasurementTermination::Exit);
    NotificationBubble::Free();
    spdlog::info("{} onDestroy", getAppName());
    delete _data.view;
    if (_data.waveform_view)
        delete _data.waveform_view;
    if (_data.live_share_view)
        delete _data.live_share_view;
}

void AppPower_monitor::_check_page_switch()
{
    // Scroll encoder to switch page
    Encoder::Update();
    if (Encoder::WasMoved())
    {
        if (_data.is_usb_c_mode)
        {
            // USB-C Mode: Fixed to Page 0 (Simple Detail) per user request
            // No toggling or scrolling allowed
            return;
        }
        else if (_data.initial_page_num == 1 || _data.initial_page_num == 2)
        {
            // Edu Mode (Volt=1 or Current=2): Fixed page, no scrolling allowed
            return;
        }
        else
        {
            // Normal scroll
            if (Encoder::GetDirection() < 0)
            {
                if (_data.current_page_num >= _total_page_num)
                    return;
                _data.current_page_num++;
                _data.is_page_switched = true;
            }
            else
            {
                if (_data.current_page_num <= 0)
                    return;
                _data.current_page_num--;
                _data.is_page_switched = true;
            }
        }
    }

    // Update page
    if (_data.is_page_switched)
    {
        _data.is_page_switched = false;

        //  Setup to new page
        spdlog::info("switch to page {}", _data.current_page_num);
        switch (_data.current_page_num)
        {
        case 0:
            // If comming from waveform
            if (_data.waveform_view)
            {
                delete _data.waveform_view;
                _data.waveform_view = nullptr;
            }
            _setup_page_simple_detail();
            HAL::NvsSet(NVS_KEY_APP_HISTORY, 0);
            break;
        case 1:
            _data.view->quitSimpleDetailPage();
            _setup_page_bus_volt();
            _data.view->reset();
            HAL::NvsSet(NVS_KEY_APP_HISTORY, 1);
            break;
        case 2:
            _setup_page_shunt_current();
            _data.view->reset();
            HAL::NvsSet(NVS_KEY_APP_HISTORY, 2);
            break;
        case 3:
            _data.view->quitMoreDetailPage();
            _setup_page_bus_power();
            _data.view->reset();
            HAL::NvsSet(NVS_KEY_APP_HISTORY, 3);
            break;
        case 4:
            _setup_page_more_detail();
            HAL::NvsSet(NVS_KEY_APP_HISTORY, 4);
            break;
        case 5:                                 // Waveform
            _data.view->quitSimpleDetailPage(); // Cleanup simple detail if needed
            _setup_page_waveform();
            HAL::NvsSet(NVS_KEY_APP_HISTORY, 5);
            break;
        default:
            break;
        }
    }
}

void AppPower_monitor::_update_view()
{
    if (HAL::Millis() - _data.pm_update_time_count > 100)
    {
        HAL::UpdatePowerMonitor();
        _data.pm_update_time_count = HAL::Millis();
    }

    if (_data.share_session.state() != LIVE_SHARE_SESSION::State::Inactive && _data.live_share_view)
    {
        _data.live_share_view->update(_data.share_session.state(),
                                      _share_theme_color(),
                                      HAL::GetSystemLiveWifiSsid(),
                                      HAL::GetSystemLiveViewerUrl(),
                                      _data.share_session.lastStartOutcome());
    }
    else if (_data.current_page_num == 5 && _data.waveform_view)
    {
        _data.waveform_view->update();
        if (_data.waveform_view->want2quit())
            destroyApp();
    }
    else
    {
        _data.view->update(HAL::Millis());
        if (_data.view->want2quit())
            destroyApp();
    }
}

bool AppPower_monitor::_is_educational_measurement() const
{
    return _data.initial_page_num == 1 || _data.initial_page_num == 2;
}

uint32_t AppPower_monitor::_share_theme_color() const
{
    return _data.initial_page_num == 1
               ? AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageBusVoltage
               : AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageShuntCurrent;
}

void AppPower_monitor::_update_share_workflow()
{
    using namespace LIVE_SHARE_SESSION;
    const std::uint32_t now = HAL::Millis();
    const State state = _data.share_session.state();

    if ((state == State::Inactive || state == State::StartError) && Button::Side()->wasClicked())
    {
        _execute_share_action(_data.share_session.requestStart());
        return;
    }

    if (state == State::WifiQr || state == State::ViewerQr)
    {
        if (Button::Side()->wasClicked())
        {
            _execute_share_action(_data.share_session.requestStop(now));
            return;
        }
        if (state == State::WifiQr)
        {
            _data.share_session.stationCountObserved(HAL::GetApStaNum());
            if (_data.share_session.state() == State::WifiQr && Button::Encoder()->wasClicked())
                _data.share_session.manualNext();
        }
        else if (Button::Encoder()->wasClicked())
        {
            // Frozen C2B option A: Viewer QR encoder click returns to Wi-Fi QR.
            _data.share_session.manualPrevious();
        }
        return;
    }

    if (state == State::Stopping)
    {
        Action action = _data.share_session.observeTransportStop(HAL::PollSystemLiveStop(), now);
        if (action == Action::None)
            action = _data.share_session.tick(now);
        _execute_share_action(action);
        return;
    }

    if (state == State::StopRecovery &&
        (Button::Encoder()->wasClicked() || Button::Side()->wasClicked()))
        _execute_share_action(_data.share_session.retryCleanup());
}

void AppPower_monitor::_execute_share_action(LIVE_SHARE_SESSION::Action action)
{
    using namespace LIVE_SHARE_SESSION;
    switch (action)
    {
    case Action::StartSystemLive:
    {
        if (_data.live_share_view == nullptr)
            _data.live_share_view = new VIEWS::LiveShareView;
        const WEB_SERVER_OWNER::StartResult result = HAL::StartSystemLiveSharing();
        StartOutcome outcome = StartOutcome::Failed;
        if (result == WEB_SERVER_OWNER::StartResult::Started)
            outcome = StartOutcome::Started;
        else if (result == WEB_SERVER_OWNER::StartResult::BusyOtherOwner)
            outcome = StartOutcome::Busy;
        _data.share_session.finishStart(outcome);
        break;
    }
    case Action::BeginNetworkStop:
    {
        const Action next =
            _data.share_session.observeTransportStop(HAL::BeginSystemLiveStop(), HAL::Millis());
        _execute_share_action(next);
        break;
    }
    case Action::StopSystemServer:
    case Action::RetryCleanup:
        _finish_share_cleanup(HAL::FinishSystemLiveStop());
        break;
    case Action::None:
    default:
        break;
    }
}

void AppPower_monitor::_finish_share_cleanup(WEB_SERVER_OWNER::StopResult result)
{
    using namespace LIVE_SHARE_SESSION;
    CleanupOutcome outcome = CleanupOutcome::RetryRequired;
    switch (result)
    {
    case WEB_SERVER_OWNER::StopResult::Stopped:
        outcome = CleanupOutcome::Stopped;
        break;
    case WEB_SERVER_OWNER::StopResult::AlreadyStopped:
        outcome = CleanupOutcome::AlreadyStopped;
        break;
    case WEB_SERVER_OWNER::StopResult::RejectedWrongOwner:
        outcome = CleanupOutcome::RejectedWrongOwner;
        break;
    case WEB_SERVER_OWNER::StopResult::ApStopFailed:
        outcome = CleanupOutcome::ApStopFailed;
        break;
    case WEB_SERVER_OWNER::StopResult::RetryRequired:
    default:
        outcome = CleanupOutcome::RetryRequired;
        break;
    }
    _data.share_session.finishCleanup(outcome);
    if (_data.share_session.state() == State::Inactive)
    {
        delete _data.live_share_view;
        _data.live_share_view = nullptr;
    }
}

void AppPower_monitor::_setup_page_bus_volt()
{
    spdlog::info("page bus volt");

    _data.view->setConfig().themeColor = AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageBusVoltage;
    _data.view->setConfig().showLcModeLabel = false;
    _data.view->setConfig().panelImage = (void*)(AssetPool::GetStaticAsset()->Image.AppPowerMonitor.page_bus_volt_panel);

    _data.view->setConfig().getValueCallback = [](std::string& text)
    { text = HAL::GetUnitAdaptatedVoltage(HAL::GetPowerMonitorData().busVoltage).value; };

    _data.view->setConfig().getLabelCallback = [](std::string& text)
    {
        text = spdlog::fmt_lib::format("{} ({})",
                                       AssetPool::GetText().AppPowerMonitor_PageSingle_InputVolt,
                                       HAL::GetUnitAdaptatedVoltage(HAL::GetPowerMonitorData().busVoltage).unit);
    };
}

void AppPower_monitor::_setup_page_bus_power()
{
    spdlog::info("page bus power");

    _data.view->setConfig().themeColor = AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageBusPower;
    _data.view->setConfig().showLcModeLabel = false;
    _data.view->setConfig().panelImage = (void*)(AssetPool::GetStaticAsset()->Image.AppPowerMonitor.page_bus_power_panel);

    _data.view->setConfig().getValueCallback = [](std::string& text)
    { text = HAL::GetUnitAdaptatedPower(HAL::GetPowerMonitorData().busPower).value; };

    _data.view->setConfig().getLabelCallback = [](std::string& text)
    {
        text = spdlog::fmt_lib::format("{} ({})",
                                       AssetPool::GetText().AppPowerMonitor_PageSingle_InputPower,
                                       HAL::GetUnitAdaptatedPower(HAL::GetPowerMonitorData().busPower).unit);
    };
}

void AppPower_monitor::_setup_page_shunt_current()
{
    spdlog::info("page shunt current");

    _data.view->setConfig().themeColor = AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageShuntCurrent;
    _data.view->setConfig().showLcModeLabel = true;
    _data.view->setConfig().panelImage = (void*)(AssetPool::GetStaticAsset()->Image.AppPowerMonitor.page_shunt_current_panel);

    _data.view->setConfig().getValueCallback = [](std::string& text)
    { text = HAL::GetUnitAdaptatedCurrent(HAL::GetPowerMonitorData().shuntCurrent).value; };

    _data.view->setConfig().getLabelCallback = [](std::string& text)
    {
        text = spdlog::fmt_lib::format("{} ({})",
                                       AssetPool::GetText().AppPowerMonitor_PageSingle_OutputCurrent,
                                       HAL::GetUnitAdaptatedCurrent(HAL::GetPowerMonitorData().shuntCurrent).unit);
    };
}

void AppPower_monitor::_setup_page_shunt_volt()
{
    spdlog::info("page shunt volt");

    _data.view->setConfig().themeColor = AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageShuntVoltage;
    _data.view->setConfig().showLcModeLabel = true;
    _data.view->setConfig().panelImage = (void*)(AssetPool::GetStaticAsset()->Image.AppPowerMonitor.page_shunt_volt_panel);

    _data.view->setConfig().getValueCallback = [](std::string& text)
    { text = HAL::GetUnitAdaptatedVoltage(HAL::GetPowerMonitorData().shuntVoltage).value; };

    _data.view->setConfig().getLabelCallback = [](std::string& text)
    {
        text = spdlog::fmt_lib::format("{} ({})",
                                       AssetPool::GetText().AppPowerMonitor_PageSingle_ShuntVolt,
                                       HAL::GetUnitAdaptatedVoltage(HAL::GetPowerMonitorData().shuntVoltage).unit);
    };
}

void AppPower_monitor::_setup_page_simple_detail()
{
    spdlog::info("page simple detail");
    _data.view->setConfig().themeColor = AssetPool::GetStaticAsset()->Color.AppPowerMonitor.pageSimpleDetailBackground;
    _data.view->reset();
    _data.view->goSimpleDetailPage();
}

void AppPower_monitor::_setup_page_more_detail()
{
    spdlog::info("page all detail");
    _data.view->goMoreDetailPage();
}

void AppPower_monitor::_setup_page_waveform()
{
    spdlog::info("page waveform");
    // Use Primary Color (Blue) for background/theme to match Power Monitor aesthetic
    // Mode 0 = Both (User requested toggling Simple <-> Waveform for USB-C, which is Both V/A)
    _data.waveform_view = new VIEWS::WaveFormRecorder(AssetPool::GetStaticAsset()->Color.AppPowerMonitor.primary, 0);
    _data.waveform_view->init();
}
