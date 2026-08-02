#include "network_web_server_recovery.h"

namespace NETWORK_WEB_SERVER_RECOVERY
{
    Action DecideStartAction(WEB_SERVER_OWNER::StartResult result)
    {
        switch (result)
        {
        case WEB_SERVER_OWNER::StartResult::Started:
            return Action::ContinueWorkflow;
        case WEB_SERVER_OWNER::StartResult::BusyOtherOwner:
            return Action::ShowBusyNotice;
        case WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry:
            return Action::EnterStopRecovery;
        case WEB_SERVER_OWNER::StartResult::AllocationOrListenFailure:
        case WEB_SERVER_OWNER::StartResult::RouteOrRegistrationFailure:
        default:
            return Action::ShowStartFailure;
        }
    }

    bool StopCompleted(WEB_SERVER_OWNER::StopResult result)
    {
        return WEB_SERVER_OWNER::IsStopSuccessful(result);
    }

    bool KeepRecovery(WEB_SERVER_OWNER::StopResult result)
    {
        return WEB_SERVER_OWNER::NeedsStopRecovery(result);
    }

    const char* StopResultMessage(WEB_SERVER_OWNER::StopResult result)
    {
        switch (result)
        {
        case WEB_SERVER_OWNER::StopResult::RetryRequired:
            return "Stop failed.";
        case WEB_SERVER_OWNER::StopResult::RejectedWrongOwner:
            return "Server is busy.";
        case WEB_SERVER_OWNER::StopResult::ApStopFailed:
            return "AP stop failed.";
        case WEB_SERVER_OWNER::StopResult::Stopped:
        case WEB_SERVER_OWNER::StopResult::AlreadyStopped:
        default:
            return "Web server stopped.";
        }
    }
} // namespace NETWORK_WEB_SERVER_RECOVERY
