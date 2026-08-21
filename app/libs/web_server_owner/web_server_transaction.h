#pragma once

#include "web_server_results.h"

#include <cstdint>

namespace WEB_SERVER_OWNER
{
    typedef bool (*StopCallback)(void* context, std::uintptr_t rawServerHandleKey);
    typedef void (*TransactionCallback)(void* context, std::uintptr_t rawServerHandleKey);

    struct TransactionRequest
    {
        State* ownerState;
        Owner requestedOwner;
        std::uintptr_t wrapperKey;
        std::uintptr_t rawServerHandleKey;
        void* context;
        TransactionCallback beforeStop;
        StopCallback stop;
        TransactionCallback clearWrapperAfterStop;
        TransactionCallback afterStop;
        TransactionCallback stopFailure;
    };

    struct PartialCleanupOutcome
    {
        StartResult result;
        std::uintptr_t wrapperKey;
    };

    struct StopTransactionOutcome
    {
        StopResult result;
        std::uintptr_t wrapperKey;
    };

    // Cleans up a listen() failure.  A null wrapper key means that listen did
    // not create an HTTPD handle and only owner release is required.
    PartialCleanupOutcome CleanupPartial(const TransactionRequest& request);

    // Performs one normal or explicit-retry stop.  The raw handle key is the
    // only value passed after a successful stop callback; wrapperKey is never
    // dereferenced by this allocation-free helper.
    StopTransactionOutcome StopOwned(const TransactionRequest& request);
} // namespace WEB_SERVER_OWNER
