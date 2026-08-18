#include "local_fault.h"

namespace LOCAL_FAULT
{
    Cause ClassifyRecorderError(RecorderErrorSource source)
    {
        switch (source)
        {
        case RecorderErrorSource::StorageInfoFailed:
        case RecorderErrorSource::InsufficientSpace:
            return Cause::NoSpace;
        case RecorderErrorSource::TempPrepareFailed:
        case RecorderErrorSource::OpenChunkFailed:
        case RecorderErrorSource::WriteChunkFailed:
        case RecorderErrorSource::OpenFinalFailed:
        case RecorderErrorSource::WriteFinalFailed:
        case RecorderErrorSource::CloseFailed:
        case RecorderErrorSource::AllocationFailed:
        case RecorderErrorSource::TaskCreateFailed:
        case RecorderErrorSource::Unknown:
        default:
            return Cause::GenericRecorderError;
        }
    }

    Presentation SelectPresentation(bool relayOffSoftwareConfirmed,
                                    bool safetyCleanupDebt,
                                    bool recorderCleanupPendingOrFailed)
    {
        if (!relayOffSoftwareConfirmed)
            return Presentation::OutputUnconfirmed;
        if (safetyCleanupDebt || recorderCleanupPendingOrFailed)
            return Presentation::CleanupPending;
        return Presentation::Normal;
    }

    Presentation SelectPresentation(const Payload& payload)
    {
        return SelectPresentation(
            payload.relayOffSoftwareConfirmed, payload.safetyCleanupDebt, payload.recorderCleanupPendingOrFailed);
    }

    Payload BuildPayload(Cause cause,
                         bool relayOffSoftwareConfirmed,
                         bool safetyCleanupDebt,
                         bool recorderCleanupPendingOrFailed)
    {
        Payload payload;
        payload.cause = cause;
        payload.relayOffSoftwareConfirmed = relayOffSoftwareConfirmed;
        payload.safetyCleanupDebt = safetyCleanupDebt;
        payload.recorderCleanupPendingOrFailed = recorderCleanupPendingOrFailed;
        payload.acknowledgementAllowed =
            SelectPresentation(relayOffSoftwareConfirmed, safetyCleanupDebt, recorderCleanupPendingOrFailed) ==
            Presentation::Normal;
        return payload;
    }

    bool CanAcknowledge(const Payload& payload, bool encoderClicked)
    {
        return payload.acknowledgementAllowed && encoderClicked;
    }
} // namespace LOCAL_FAULT
