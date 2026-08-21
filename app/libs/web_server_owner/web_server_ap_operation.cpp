#include "web_server_ap_operation.h"

namespace WEB_SERVER_OWNER
{
    namespace
    {
        ApModeState QueryApMode(const ApOperationCallbacks& callbacks)
        {
            if (callbacks.queryApMode == nullptr)
                return ApModeState::Unknown;

            const ApModeState mode = callbacks.queryApMode(callbacks.context);
            if (mode == ApModeState::Disabled || mode == ApModeState::Enabled)
                return mode;
            return ApModeState::Unknown;
        }
    } // namespace

    ApOperation::ApOperation() : _state(ApState::Inactive) {}

    bool ApOperation::reconcileStartPreflight(const ApOperationCallbacks& callbacks)
    {
        const ApModeState mode = QueryApMode(callbacks);
        if (mode == ApModeState::Disabled)
        {
            _state = ApState::Inactive;
            return true;
        }

        _state = ApState::StopRetryRequired;
        return false;
    }

    ApStartResult ApOperation::start(const ApOperationCallbacks& callbacks)
    {
        if (!reconcileStartPreflight(callbacks))
            return ApStartResult::StopRetryRequired;

        if (callbacks.isStaConnected == nullptr)
        {
            _state = ApState::Inactive;
            return ApStartResult::StartFailed;
        }
        if (callbacks.startAp == nullptr)
        {
            _state = ApState::Inactive;
            return ApStartResult::StartFailed;
        }

        const bool staConnected = callbacks.isStaConnected(callbacks.context);
        if (staConnected && (callbacks.disconnectSta == nullptr || !callbacks.disconnectSta(callbacks.context)))
        {
            _state = ApState::Inactive;
            return ApStartResult::StaDisconnectFailed;
        }

        const bool startSucceeded = callbacks.startAp(callbacks.context);
        const ApModeState postStartMode = QueryApMode(callbacks);
        if (startSucceeded && postStartMode == ApModeState::Enabled)
        {
            _state = ApState::Active;
            return ApStartResult::Started;
        }

        if (!startSucceeded && postStartMode == ApModeState::Disabled)
        {
            _state = ApState::Inactive;
            return ApStartResult::StartFailed;
        }

        if (callbacks.stopAp == nullptr || !callbacks.stopAp(callbacks.context))
        {
            _state = ApState::StopRetryRequired;
            return ApStartResult::StopRetryRequired;
        }

        const ApModeState postCleanupMode = QueryApMode(callbacks);
        if (postCleanupMode == ApModeState::Disabled)
        {
            _state = ApState::Inactive;
            return ApStartResult::StartFailed;
        }

        _state = ApState::StopRetryRequired;
        return ApStartResult::StopRetryRequired;
    }

    ApStopResult ApOperation::stop(const ApOperationCallbacks& callbacks)
    {
        const ApModeState preStopMode = QueryApMode(callbacks);
        if (preStopMode == ApModeState::Disabled)
        {
            _state = ApState::Inactive;
            return ApStopResult::AlreadyStopped;
        }

        if (callbacks.stopAp == nullptr || !callbacks.stopAp(callbacks.context))
        {
            _state = ApState::StopRetryRequired;
            return ApStopResult::StopFailed;
        }

        const ApModeState postStopMode = QueryApMode(callbacks);
        if (postStopMode == ApModeState::Disabled)
        {
            _state = ApState::Inactive;
            return ApStopResult::Stopped;
        }

        _state = ApState::StopRetryRequired;
        return ApStopResult::StopFailed;
    }

    ApState ApOperation::state() const { return _state; }

    bool ApOperation::stopRetryRequired() const { return _state == ApState::StopRetryRequired; }
} // namespace WEB_SERVER_OWNER
