#include "web_server_results.h"

namespace WEB_SERVER_OWNER
{
    StartPreflightResult StartPreflight(const State& state, Owner requestedOwner, bool retainedServer)
    {
        if (requestedOwner == Owner::None)
            return StartPreflightResult::BusyOtherOwner;

        if (state.owner() == Owner::None)
            return retainedServer ? StartPreflightResult::RetainedServerNeedsStopRetry
                                   : StartPreflightResult::Proceed;

        if (requestedOwner == Owner::System && state.owner() == Owner::System && state.retained())
            return StartPreflightResult::RetainedServerNeedsStopRetry;

        return StartPreflightResult::BusyOtherOwner;
    }

    bool IsStartSuccessful(StartResult result) { return result == StartResult::Started; }

    bool IsStopSuccessful(StopResult result)
    {
        return result == StopResult::Stopped || result == StopResult::AlreadyStopped;
    }

    bool ShouldStopApAfterStartFailure(StartResult result)
    {
        return result == StartResult::AllocationOrListenFailure || result == StartResult::RouteOrRegistrationFailure;
    }

    bool NeedsStopRecovery(StopResult result)
    {
        return result == StopResult::RetryRequired || result == StopResult::RejectedWrongOwner ||
               result == StopResult::ApStopFailed;
    }
} // namespace WEB_SERVER_OWNER
