#include "live_share_safety.h"

namespace LIVE_SHARE_SAFETY
{
    namespace
    {
        bool Invoke(ActionCallback callback, void* context)
        {
            return callback != nullptr && callback(context);
        }
    } // namespace

    bool Result::hasCleanupDebt() const
    {
        if (!executable)
            return false;
        if (!relayOffConfirmed || !producerInvalidated || !transportTerminationDispositionRecorded ||
            !admissionDisabled)
            return true;
        if (httpdStopDisposition == HttpdStopDisposition::RetryRequired ||
            httpdStopDisposition == HttpdStopDisposition::RejectedWrongOwner)
            return true;
        return !apStopAttempted || !apStopped;
    }

    bool Result::teardownComplete() const { return executable && !hasCleanupDebt(); }

    Result Execute(TerminationReason reason, const Callbacks& callbacks)
    {
        Result result = {
            reason,
            reason == TerminationReason::MeasurementExit || reason == TerminationReason::MeasurementFault,
            false,
            false,
            false,
            false,
            false,
            false,
            false,
            false,
            false,
            HttpdStopDisposition::RetryRequired,
            false,
            false,
        };
        if (!result.executable)
            return result;

        result.relayOffAttempted = true;
        result.relayOffConfirmed = Invoke(callbacks.relayOff, callbacks.context);

        result.producerInvalidateAttempted = true;
        result.producerInvalidated = Invoke(callbacks.producerInvalidate, callbacks.context);

        result.transportTerminationAttempted = true;
        result.transportTerminationDispositionRecorded =
            Invoke(callbacks.bestEffortTransportTermination, callbacks.context);

        result.admissionOffAttempted = true;
        result.admissionDisabled = Invoke(callbacks.admissionOff, callbacks.context);

        result.httpdStopAttempted = true;
        if (callbacks.httpdStop != nullptr)
            result.httpdStopDisposition = callbacks.httpdStop(callbacks.context);

        if (result.httpdStopDisposition == HttpdStopDisposition::Stopped ||
            result.httpdStopDisposition == HttpdStopDisposition::AlreadyStopped)
        {
            result.apStopAttempted = true;
            result.apStopped = Invoke(callbacks.apStop, callbacks.context);
        }

        return result;
    }
} // namespace LIVE_SHARE_SAFETY
