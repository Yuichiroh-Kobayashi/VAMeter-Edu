#include "live_share_controller.h"

namespace LIVE_SHARE_CONTROLLER
{
    bool CanStartLiveView(RecorderActivity recorderActivity, LIVE_SHARE_SESSION::State liveShareState)
    {
        return recorderActivity == RecorderActivity::Idle && liveShareState == LIVE_SHARE_SESSION::State::Inactive;
    }

    bool CanStartRecording(RecorderActivity recorderActivity, LIVE_SHARE_SESSION::State liveShareState)
    {
        return recorderActivity == RecorderActivity::Idle && liveShareState == LIVE_SHARE_SESSION::State::Inactive;
    }

    ForegroundAction ResolveWaveformInput(const InputSnapshot& input,
                                          RecorderActivity recorderActivity,
                                          LIVE_SHARE_SESSION::State liveShareState,
                                          bool recordingInputAvailable)
    {
        if (liveShareState != LIVE_SHARE_SESSION::State::Inactive)
            return ForegroundAction::None;
        if (input.sideHeld)
            return recorderActivity == RecorderActivity::Saving ? ForegroundAction::None : ForegroundAction::Exit;
        if (input.sideClicked && input.encoderClicked)
            return ForegroundAction::None;
        if (recorderActivity == RecorderActivity::BusyCleanup)
            return input.encoderClicked ? ForegroundAction::RetryRecorderCleanup : ForegroundAction::None;
        if (recorderActivity != RecorderActivity::Idle)
            return ForegroundAction::None;
        if (input.sideClicked)
            return ForegroundAction::StartLiveView;
        if (input.encoderClicked && recordingInputAvailable)
            return ForegroundAction::StartRecording;
        return ForegroundAction::None;
    }

    LIVE_SHARE_SESSION::StartOutcome MapStartResult(WEB_SERVER_OWNER::StartResult result)
    {
        using LIVE_SHARE_SESSION::StartOutcome;
        switch (result)
        {
        case WEB_SERVER_OWNER::StartResult::Started:
            return StartOutcome::Started;
        case WEB_SERVER_OWNER::StartResult::BusyOtherOwner:
            return StartOutcome::BusyOtherOwner;
        case WEB_SERVER_OWNER::StartResult::ApStartFailed:
            return StartOutcome::ApStartFailed;
        case WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry:
            return StartOutcome::RetainedApNeedsStopRetry;
        case WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry:
            return StartOutcome::RetainedServerNeedsStopRetry;
        case WEB_SERVER_OWNER::StartResult::AllocationOrListenFailure:
            return StartOutcome::AllocationOrListenFailure;
        case WEB_SERVER_OWNER::StartResult::RouteOrRegistrationFailure:
            return StartOutcome::RouteOrRegistrationFailure;
        default:
            return StartOutcome::AllocationOrListenFailure;
        }
    }

    LiveShareController::LiveShareController(const TransportCallbacks& callbacks) : _callbacks(callbacks) {}

    LIVE_SHARE_SESSION::State LiveShareController::state() const { return _session.state(); }

    LIVE_SHARE_SESSION::StartOutcome LiveShareController::lastStartOutcome() const { return _session.lastStartOutcome(); }

    bool LiveShareController::isInactive() const { return _session.isInactive(); }

    bool LiveShareController::ownsInteraction() const { return !_session.isInactive(); }

    bool LiveShareController::requestStart(RecorderActivity recorderActivity)
    {
        if (!CanStartLiveView(recorderActivity, _session.state()))
            return false;
        if (_session.requestStart() != LIVE_SHARE_SESSION::Action::StartSystemLive)
            return false;

        const WEB_SERVER_OWNER::StartResult result = _callbacks.startSystemLive == nullptr
                                                         ? WEB_SERVER_OWNER::StartResult::AllocationOrListenFailure
                                                         : _callbacks.startSystemLive(_callbacks.context);
        _session.finishStart(MapStartResult(result));

        if (_session.state() == LIVE_SHARE_SESSION::State::StopRecovery)
        {
            const WEB_SERVER_OWNER::StopResult cleanupResult = _callbacks.finishSystemLiveStop == nullptr
                                                                   ? WEB_SERVER_OWNER::StopResult::RetryRequired
                                                                   : _callbacks.finishSystemLiveStop(_callbacks.context);
            finishCleanup(cleanupResult);
        }
        return true;
    }

    void LiveShareController::update(const InputSnapshot& input)
    {
        using namespace LIVE_SHARE_SESSION;
        const State entryState = _session.state();

        if (entryState == State::StartError)
        {
            if (input.sideClicked)
                _session.dismissStartError();
            return;
        }

        if (entryState == State::WifiQr || entryState == State::ViewerQr)
        {
            if (input.sideClicked)
            {
                execute(_session.requestStop(nowMs()));
                return;
            }
            if (entryState == State::WifiQr)
            {
                if (_callbacks.stationCount != nullptr)
                    _session.stationCountObserved(_callbacks.stationCount(_callbacks.context));
                if (_session.state() == State::WifiQr && input.encoderClicked)
                    _session.manualNext();
            }
            else if (input.encoderClicked)
            {
                _session.manualPrevious();
            }
            return;
        }

        if (entryState == State::Stopping)
        {
            const TransportStopStatus status = _callbacks.pollSystemLiveStop == nullptr
                                                   ? TransportStopStatus::Failed
                                                   : _callbacks.pollSystemLiveStop(_callbacks.context);
            Action action = _session.observeTransportStop(status, nowMs());
            if (action == Action::None)
                action = _session.tick(nowMs());
            execute(action);
            return;
        }

        if (entryState == State::StopRecovery && (input.encoderClicked || input.sideClicked))
            execute(_session.retryCleanup());
    }

    void LiveShareController::measurementTerminationObserved(LIVE_SHARE_SESSION::MeasurementTermination reason)
    {
        _session.measurementTerminationObserved(reason);
    }

    std::uint32_t LiveShareController::nowMs() const
    {
        return _callbacks.millis == nullptr ? 0U : _callbacks.millis(_callbacks.context);
    }

    void LiveShareController::execute(LIVE_SHARE_SESSION::Action action)
    {
        using namespace LIVE_SHARE_SESSION;
        switch (action)
        {
        case Action::BeginNetworkStop:
        {
            const TransportStopStatus status = _callbacks.beginSystemLiveStop == nullptr
                                                   ? TransportStopStatus::Failed
                                                   : _callbacks.beginSystemLiveStop(_callbacks.context);
            execute(_session.observeTransportStop(status, nowMs()));
            break;
        }
        case Action::StopSystemServer:
        case Action::RetryCleanup:
        {
            const WEB_SERVER_OWNER::StopResult result = _callbacks.finishSystemLiveStop == nullptr
                                                            ? WEB_SERVER_OWNER::StopResult::RetryRequired
                                                            : _callbacks.finishSystemLiveStop(_callbacks.context);
            finishCleanup(result);
            break;
        }
        case Action::StartSystemLive:
        case Action::None:
        default:
            break;
        }
    }

    void LiveShareController::finishCleanup(WEB_SERVER_OWNER::StopResult result)
    {
        using LIVE_SHARE_SESSION::CleanupOutcome;
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
        _session.finishCleanup(outcome);
    }
} // namespace LIVE_SHARE_CONTROLLER
