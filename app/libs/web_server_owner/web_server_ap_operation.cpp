#include "web_server_ap_operation.h"

namespace WEB_SERVER_OWNER
{
    ApOperation::ApOperation() : _state(ApState::Inactive) {}

    ApStartResult ApOperation::start(const ApOperationCallbacks& callbacks)
    {
        if (_state != ApState::Inactive)
            return ApStartResult::StopRetryRequired;

        // These callbacks are required before any Wi-Fi side effect.  A
        // missing callback is therefore a bounded, clean failure.
        if (callbacks.isStaConnected == nullptr || callbacks.startAp == nullptr)
            return ApStartResult::StartFailed;

        const bool staConnected = callbacks.isStaConnected(callbacks.context);
        if (staConnected)
        {
            if (callbacks.disconnectSta == nullptr || !callbacks.disconnectSta(callbacks.context))
                return ApStartResult::StaDisconnectFailed;
        }

        if (callbacks.startAp(callbacks.context))
        {
            _state = ApState::Active;
            return ApStartResult::Started;
        }

        // softAP() may leave a partial AP interface behind.  Confirm the
        // state before deciding whether cleanup is needed.
        if (callbacks.isApStarted == nullptr)
        {
            _state = ApState::StopRetryRequired;
            return ApStartResult::StopRetryRequired;
        }
        if (!callbacks.isApStarted(callbacks.context))
        {
            _state = ApState::Inactive;
            return ApStartResult::StartFailed;
        }
        if (callbacks.stopAp == nullptr)
        {
            _state = ApState::StopRetryRequired;
            return ApStartResult::StopRetryRequired;
        }
        if (callbacks.stopAp(callbacks.context))
        {
            _state = ApState::Inactive;
            return ApStartResult::StartFailed;
        }

        _state = ApState::StopRetryRequired;
        return ApStartResult::StopRetryRequired;
    }

    ApStopResult ApOperation::stop(const ApOperationCallbacks& callbacks)
    {
        if (_state == ApState::Inactive)
            return ApStopResult::AlreadyStopped;

        if (callbacks.isApStarted == nullptr)
        {
            _state = ApState::StopRetryRequired;
            return ApStopResult::StopFailed;
        }
        if (!callbacks.isApStarted(callbacks.context))
        {
            _state = ApState::Inactive;
            return ApStopResult::AlreadyStopped;
        }
        if (callbacks.stopAp == nullptr || !callbacks.stopAp(callbacks.context))
        {
            _state = ApState::StopRetryRequired;
            return ApStopResult::StopFailed;
        }

        _state = ApState::Inactive;
        return ApStopResult::Stopped;
    }

    ApState ApOperation::state() const { return _state; }

    bool ApOperation::stopRetryRequired() const { return _state == ApState::StopRetryRequired; }
} // namespace WEB_SERVER_OWNER
