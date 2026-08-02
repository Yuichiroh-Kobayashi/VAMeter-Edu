#pragma once

#include "web_server_owner.h"

#include <cstdint>

namespace WEB_SERVER_OWNER
{
    enum class StartResult : std::uint8_t
    {
        Started,
        BusyOtherOwner,
        RetainedServerNeedsStopRetry,
        AllocationOrListenFailure,
        RouteOrRegistrationFailure,
    };

    enum class StopResult : std::uint8_t
    {
        Stopped,
        AlreadyStopped,
        RetryRequired,
        RejectedWrongOwner,
        ApStopFailed,
    };

    enum class StartPreflightResult : std::uint8_t
    {
        Proceed,
        BusyOtherOwner,
        RetainedServerNeedsStopRetry,
    };

    StartPreflightResult StartPreflight(const State& state,
                                        Owner requestedOwner,
                                        bool retainedServer);

    bool IsStartSuccessful(StartResult result);
    bool IsStopSuccessful(StopResult result);

    // A failed start may stop the AP only after all server ownership has been
    // released.  Busy and retained states deliberately preserve the AP.
    bool ShouldStopApAfterStartFailure(StartResult result);

    // A server stop result is mapped to the user-visible recovery decision.
    bool NeedsStopRecovery(StopResult result);
} // namespace WEB_SERVER_OWNER
