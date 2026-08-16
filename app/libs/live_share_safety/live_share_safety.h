#pragma once

#include <cstdint>

namespace LIVE_SHARE_SAFETY
{
    enum class TerminationReason : std::uint8_t
    {
        MeasurementExit,
        MeasurementFault,
        StopSharing,
    };

    enum class Action : std::uint8_t
    {
        RelayOff,
        ProducerInvalidate,
        BestEffortTransportTermination,
        AdmissionOff,
        HttpdStop,
        ApStop,
    };

    enum class HttpdStopDisposition : std::uint8_t
    {
        Stopped,
        AlreadyStopped,
        RetryRequired,
        RejectedWrongOwner,
    };

    typedef bool (*ActionCallback)(void* context);
    typedef HttpdStopDisposition (*HttpdStopCallback)(void* context);

    struct Callbacks
    {
        void* context;
        ActionCallback relayOff;
        ActionCallback producerInvalidate;
        ActionCallback bestEffortTransportTermination;
        ActionCallback admissionOff;
        HttpdStopCallback httpdStop;
        ActionCallback apStop;
    };

    struct Result
    {
        TerminationReason reason;
        bool executable;
        bool relayOffAttempted;
        bool relayOffConfirmed;
        bool producerInvalidateAttempted;
        bool producerInvalidated;
        bool transportTerminationAttempted;
        bool transportTerminationDispositionRecorded;
        bool admissionOffAttempted;
        bool admissionDisabled;
        bool httpdStopAttempted;
        HttpdStopDisposition httpdStopDisposition;
        bool apStopAttempted;
        bool apStopped;

        bool hasCleanupDebt() const;
        bool teardownComplete() const;
    };

    Result Execute(TerminationReason reason, const Callbacks& callbacks);
} // namespace LIVE_SHARE_SAFETY
