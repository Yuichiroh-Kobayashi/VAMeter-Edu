#pragma once

#include "../live_share_session/live_share_session.h"
#include "../web_server_owner/web_server_results.h"

#include <cstdint>

namespace LIVE_SHARE_CONTROLLER
{
    enum class RecorderActivity : std::uint8_t
    {
        Idle,
        WaitingTrigger,
        Recording,
        Saving,
        BusyCleanup,
        FaultLatched,
    };

    struct InputSnapshot
    {
        bool sideClicked = false;
        bool sideHeld = false;
        bool encoderClicked = false;
        bool encoderHeld = false;
    };

    enum class ForegroundAction : std::uint8_t
    {
        None,
        StartLiveView,
        StartRecording,
        RetryRecorderCleanup,
        Exit,
    };

    bool CanStartLiveView(RecorderActivity recorderActivity, LIVE_SHARE_SESSION::State liveShareState);
    bool CanStartRecording(RecorderActivity recorderActivity, LIVE_SHARE_SESSION::State liveShareState);
    ForegroundAction ResolveWaveformInput(const InputSnapshot& input,
                                          RecorderActivity recorderActivity,
                                          LIVE_SHARE_SESSION::State liveShareState,
                                          bool recordingInputAvailable);
    LIVE_SHARE_SESSION::StartOutcome MapStartResult(WEB_SERVER_OWNER::StartResult result);

    struct TransportCallbacks
    {
        void* context = nullptr;
        WEB_SERVER_OWNER::StartResult (*startSystemLive)(void*) = nullptr;
        LIVE_SHARE_SESSION::TransportStopStatus (*beginSystemLiveStop)(void*) = nullptr;
        LIVE_SHARE_SESSION::TransportStopStatus (*pollSystemLiveStop)(void*) = nullptr;
        WEB_SERVER_OWNER::StopResult (*finishSystemLiveStop)(void*) = nullptr;
        std::uint32_t (*millis)(void*) = nullptr;
        std::uint8_t (*stationCount)(void*) = nullptr;
    };

    class LiveShareController
    {
    public:
        explicit LiveShareController(const TransportCallbacks& callbacks);

        LIVE_SHARE_SESSION::State state() const;
        LIVE_SHARE_SESSION::StartOutcome lastStartOutcome() const;
        bool isInactive() const;
        bool ownsInteraction() const;

        bool requestStart(RecorderActivity recorderActivity);
        void update(const InputSnapshot& input);
        void measurementTerminationObserved(LIVE_SHARE_SESSION::MeasurementTermination reason);

    private:
        std::uint32_t nowMs() const;
        void execute(LIVE_SHARE_SESSION::Action action);
        void finishCleanup(WEB_SERVER_OWNER::StopResult result);

        LIVE_SHARE_SESSION::NetworkShareSession _session;
        TransportCallbacks _callbacks;
    };
} // namespace LIVE_SHARE_CONTROLLER
