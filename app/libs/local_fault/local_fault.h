#pragma once

#include <cstdint>

namespace LOCAL_FAULT
{
    // Mirrors the 10 non-`error_none` VA_RECORDER::Error_t values. Kept
    // decoupled from app/hal/types.h so this module stays host-testable
    // without pulling in HAL/toolkit headers; the HAL-facing caller
    // translates VA_RECORDER::Error_t into this enum at the boundary.
    enum class RecorderErrorSource : std::uint8_t
    {
        StorageInfoFailed,
        InsufficientSpace,
        TempPrepareFailed,
        OpenChunkFailed,
        WriteChunkFailed,
        OpenFinalFailed,
        WriteFinalFailed,
        CloseFailed,
        AllocationFailed,
        TaskCreateFailed,
        Unknown,
    };

    enum class Cause : std::uint8_t
    {
        NoSpace,
        StartOrTriggerCreationFailed,
        GenericRecorderError,
    };

    enum class Presentation : std::uint8_t
    {
        Normal,
        OutputUnconfirmed,
        CleanupPending,
    };

    struct Payload
    {
        Cause cause = Cause::GenericRecorderError;
        bool relayOffSoftwareConfirmed = false;
        bool safetyCleanupDebt = false;
        bool recorderCleanupPendingOrFailed = false;
        bool acknowledgementAllowed = false;
    };

    // Pure enum -> cause mapping. Preserves the existing NoSpace/generic
    // distinction; StorageInfoFailed and InsufficientSpace are the only two
    // sources that classify as NoSpace, matching the current
    // `error == error_insufficient_space || error == error_storage_info_failed`
    // branch this replaces.
    Cause ClassifyRecorderError(RecorderErrorSource source);

    // Fixed precedence: an unconfirmed Relay-OFF readback always wins over a
    // cleanup-debt/pending-cleanup presentation, and both win over Normal.
    Presentation SelectPresentation(bool relayOffSoftwareConfirmed,
                                    bool safetyCleanupDebt,
                                    bool recorderCleanupPendingOrFailed);
    Presentation SelectPresentation(const Payload& payload);

    // Fixed-size payload builder; never allocates.
    Payload BuildPayload(Cause cause,
                         bool relayOffSoftwareConfirmed,
                         bool safetyCleanupDebt,
                         bool recorderCleanupPendingOrFailed);

    // Single acknowledge gate: true only when the payload allows it AND the
    // caller reports a fresh Encoder-click edge. Callers must never pass
    // Side click/hold or Encoder-hold state into `encoderClicked`.
    bool CanAcknowledge(const Payload& payload, bool encoderClicked);
} // namespace LOCAL_FAULT
