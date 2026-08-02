#include "web_server_transaction.h"

namespace WEB_SERVER_OWNER
{
    namespace
    {
        void Retain(State* state)
        {
            if (state != nullptr)
                state->markRetained();
        }

        bool ValidRequest(const TransactionRequest& request)
        {
            return request.ownerState != nullptr && request.requestedOwner != Owner::None;
        }

        bool StopSucceeded(const TransactionRequest& request)
        {
            return request.stop != nullptr && request.rawServerHandleKey != 0 &&
                   request.stop(request.context, request.rawServerHandleKey);
        }
    } // namespace

    PartialCleanupOutcome CleanupPartial(const TransactionRequest& request)
    {
        PartialCleanupOutcome outcome = {StartResult::RetainedServerNeedsStopRetry, request.wrapperKey};
        if (!ValidRequest(request))
            return outcome;

        if (request.ownerState->owner() != request.requestedOwner)
            return outcome;

        if (request.wrapperKey == 0)
        {
            const bool released = request.ownerState->release(request.requestedOwner);
            if (!released)
                Retain(request.ownerState);
            outcome.result = released ? StartResult::AllocationOrListenFailure
                                      : StartResult::RetainedServerNeedsStopRetry;
            outcome.wrapperKey = 0;
            return outcome;
        }

        if (request.rawServerHandleKey == 0 || !StopSucceeded(request))
        {
            Retain(request.ownerState);
            return outcome;
        }

        // The stop callback has consumed the raw handle.  No wrapper pointer
        // is touched here or by any callback after this point.
        outcome.wrapperKey = 0;
        if (request.afterStop != nullptr)
            request.afterStop(request.context, request.rawServerHandleKey);
        const bool released = request.ownerState->release(request.requestedOwner);
        if (!released)
        {
            Retain(request.ownerState);
            return outcome;
        }
        outcome.result = StartResult::AllocationOrListenFailure;
        return outcome;
    }

    StopTransactionOutcome StopOwned(const TransactionRequest& request)
    {
        StopTransactionOutcome outcome = {StopResult::RetryRequired, request.wrapperKey};
        if (!ValidRequest(request))
            return outcome;

        const Owner currentOwner = request.ownerState->owner();
        if (currentOwner == Owner::None)
        {
            outcome.result = request.wrapperKey == 0 ? StopResult::AlreadyStopped : StopResult::RetryRequired;
            return outcome;
        }
        if (currentOwner != request.requestedOwner)
        {
            outcome.result = StopResult::RejectedWrongOwner;
            return outcome;
        }
        if (request.wrapperKey == 0 || request.rawServerHandleKey == 0)
        {
            Retain(request.ownerState);
            return outcome;
        }

        if (request.beforeStop != nullptr)
            request.beforeStop(request.context, request.rawServerHandleKey);
        if (!StopSucceeded(request))
        {
            Retain(request.ownerState);
            if (request.stopFailure != nullptr)
                request.stopFailure(request.context, request.rawServerHandleKey);
            return outcome;
        }

        // Capture is complete and the stop callback succeeded.  Clear the
        // opaque wrapper key before lifecycle cleanup and owner release.  The
        // production callback clears its global wrapper pointer here; the
        // helper itself never dereferences or deletes that wrapper.
        outcome.wrapperKey = 0;
        if (request.clearWrapperAfterStop != nullptr)
            request.clearWrapperAfterStop(request.context, request.rawServerHandleKey);
        if (request.afterStop != nullptr)
            request.afterStop(request.context, request.rawServerHandleKey);
        const bool released = request.ownerState->release(request.requestedOwner);
        if (!released)
        {
            Retain(request.ownerState);
            return outcome;
        }
        outcome.result = StopResult::Stopped;
        return outcome;
    }
} // namespace WEB_SERVER_OWNER
