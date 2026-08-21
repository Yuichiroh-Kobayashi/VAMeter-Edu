#include "local_fault.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace LF = LOCAL_FAULT;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    // Requirement #1: each of the 10 unique VA_RECORDER::Error_t fault
    // values (mirrored here as RecorderErrorSource) maps to a stable cause,
    // preserving the existing NoSpace-vs-generic distinction.
    void TestClassifyRecorderError_NoSpaceSources()
    {
        Require(LF::ClassifyRecorderError(LF::RecorderErrorSource::StorageInfoFailed) == LF::Cause::NoSpace,
                "StorageInfoFailed must classify as NoSpace");
        Require(LF::ClassifyRecorderError(LF::RecorderErrorSource::InsufficientSpace) == LF::Cause::NoSpace,
                "InsufficientSpace must classify as NoSpace");
    }

    void TestClassifyRecorderError_GenericSources()
    {
        const LF::RecorderErrorSource generic_sources[] = {
            LF::RecorderErrorSource::TempPrepareFailed,
            LF::RecorderErrorSource::OpenChunkFailed,
            LF::RecorderErrorSource::WriteChunkFailed,
            LF::RecorderErrorSource::OpenFinalFailed,
            LF::RecorderErrorSource::WriteFinalFailed,
            LF::RecorderErrorSource::CloseFailed,
            LF::RecorderErrorSource::AllocationFailed,
            LF::RecorderErrorSource::TaskCreateFailed,
        };
        for (LF::RecorderErrorSource source : generic_sources)
            Require(LF::ClassifyRecorderError(source) == LF::Cause::GenericRecorderError,
                    "all 8 remaining VA_RECORDER::Error_t sources must classify as GenericRecorderError");
    }

    // The non-enum trigger/creation failure case must never be classified
    // via ClassifyRecorderError (it has no VA_RECORDER::Error_t at all);
    // it is represented as its own distinct Cause value by the caller.
    void TestStartOrTriggerCreationFailedIsDistinct()
    {
        const LF::Payload payload = LF::BuildPayload(LF::Cause::StartOrTriggerCreationFailed, true, false, false);
        Require(payload.cause == LF::Cause::StartOrTriggerCreationFailed,
                "start/trigger creation failure must retain its own distinct cause, not error_none-as-NoSpace/Generic");
        Require(payload.cause != LF::Cause::NoSpace, "start/trigger creation failure must not collapse into NoSpace");
    }

    // Requirement #9/#10/#11: presentation precedence and ack gating.
    void TestPresentation_NormalWhenAllClear()
    {
        Require(LF::SelectPresentation(true, false, false) == LF::Presentation::Normal,
                "relay confirmed, no cleanup debt, no pending cleanup => Normal");
    }

    void TestPresentation_OutputUnconfirmedTakesPriority()
    {
        // Even with cleanup debt/pending also true, unconfirmed Relay OFF
        // must win: never claim unconditional "measurement output: OFF".
        Require(LF::SelectPresentation(false, true, true) == LF::Presentation::OutputUnconfirmed,
                "unconfirmed relay OFF must take priority over cleanup-debt presentation");
        Require(LF::SelectPresentation(false, false, false) == LF::Presentation::OutputUnconfirmed,
                "unconfirmed relay OFF alone must select OutputUnconfirmed");
    }

    void TestPresentation_CleanupPendingWhenRelayConfirmedButDebtOrPending()
    {
        Require(LF::SelectPresentation(true, true, false) == LF::Presentation::CleanupPending,
                "relay confirmed but safety cleanup debt => CleanupPending");
        Require(LF::SelectPresentation(true, false, true) == LF::Presentation::CleanupPending,
                "relay confirmed but recorder cleanup pending/failed => CleanupPending");
    }

    void TestBuildPayload_RetainsRelayOffResult()
    {
        // Requirement #3: the LIVE_SHARE_SAFETY::Result is no longer
        // discarded; relayOffSoftwareConfirmed must be retained verbatim.
        const LF::Payload confirmed = LF::BuildPayload(LF::Cause::GenericRecorderError, true, false, false);
        const LF::Payload unconfirmed = LF::BuildPayload(LF::Cause::GenericRecorderError, false, false, false);
        Require(confirmed.relayOffSoftwareConfirmed, "payload must retain a confirmed Relay OFF result");
        Require(!unconfirmed.relayOffSoftwareConfirmed, "payload must retain an unconfirmed Relay OFF result");
    }

    void TestBuildPayload_RetainsCleanupDebt()
    {
        // Requirement #4: hasCleanupDebt() must be retained, not discarded.
        const LF::Payload debt = LF::BuildPayload(LF::Cause::GenericRecorderError, true, true, false);
        const LF::Payload no_debt = LF::BuildPayload(LF::Cause::GenericRecorderError, true, false, false);
        Require(debt.safetyCleanupDebt, "payload must retain safety cleanup debt when present");
        Require(!no_debt.safetyCleanupDebt, "payload must retain absence of safety cleanup debt");
    }

    void TestBuildPayload_RetainsRecorderCleanupPending()
    {
        // Requirement #5: recorder cleanup pending/timed-out must be retained.
        const LF::Payload pending = LF::BuildPayload(LF::Cause::GenericRecorderError, true, false, true);
        const LF::Payload settled = LF::BuildPayload(LF::Cause::GenericRecorderError, true, false, false);
        Require(pending.recorderCleanupPendingOrFailed, "payload must retain recorder cleanup pending/failed state");
        Require(!settled.recorderCleanupPendingOrFailed, "payload must retain settled recorder cleanup state");
    }

    // T6/T7/T8: Relay-OFF unconfirmed, safety cleanup debt, and recorder
    // cleanup pending/failed each independently block acknowledgementAllowed
    // (SelectPresentation's fixed precedence already pins the OutputUnconfirmed
    // vs CleanupPending ordering above; this pins the ack-gate consequence).
    void TestBuildPayload_AcknowledgementAllowedOnlyWhenAllClear()
    {
        Require(LF::BuildPayload(LF::Cause::GenericRecorderError, true, false, false).acknowledgementAllowed,
                "ack must be allowed when relay confirmed, no debt, no pending cleanup");
        Require(!LF::BuildPayload(LF::Cause::GenericRecorderError, false, false, false).acknowledgementAllowed,
                "ack must be blocked when relay OFF is unconfirmed"); // T6
        Require(!LF::BuildPayload(LF::Cause::GenericRecorderError, true, true, false).acknowledgementAllowed,
                "ack must be blocked when safety cleanup debt is outstanding"); // T7
        Require(!LF::BuildPayload(LF::Cause::GenericRecorderError, true, false, true).acknowledgementAllowed,
                "ack must be blocked when recorder cleanup is pending/failed"); // T8
    }

    // T4/T5 and requirement #6/#8: CanAcknowledge only reacts to a fresh
    // encoder-click edge, gated by acknowledgementAllowed. The function
    // signature itself has no side-click/side-hold/encoder-hold
    // parameters, so those inputs structurally cannot reach this gate --
    // callers (WaveFormRecorder) must never route them here. Freshness of
    // the click edge itself (residue before/through fault entry) is a
    // WaveFormRecorder-level concern (see recorder.cpp
    // _data.fault_ack_pending_release) outside this pure module's scope.
    void TestCanAcknowledge_RequiresBothAllowedAndClick()
    {
        const LF::Payload allowed = LF::BuildPayload(LF::Cause::GenericRecorderError, true, false, false);
        const LF::Payload blocked = LF::BuildPayload(LF::Cause::GenericRecorderError, false, false, false);

        Require(LF::CanAcknowledge(allowed, true), "ack allowed + encoder click => acknowledge"); // T5
        Require(!LF::CanAcknowledge(allowed, false), "ack allowed + no click => must not acknowledge (no stale edge)"); // T4
        Require(!LF::CanAcknowledge(blocked, true), "ack blocked (unconfirmed relay) + click => must not acknowledge"); // T4
    }
} // namespace

int main()
{
    TestClassifyRecorderError_NoSpaceSources();
    TestClassifyRecorderError_GenericSources();
    TestStartOrTriggerCreationFailedIsDistinct();
    TestPresentation_NormalWhenAllClear();
    TestPresentation_OutputUnconfirmedTakesPriority();
    TestPresentation_CleanupPendingWhenRelayConfirmedButDebtOrPending();
    TestBuildPayload_RetainsRelayOffResult();
    TestBuildPayload_RetainsCleanupDebt();
    TestBuildPayload_RetainsRecorderCleanupPending();
    TestBuildPayload_AcknowledgementAllowedOnlyWhenAllClear();
    TestCanAcknowledge_RequiresBothAllowedAndClick();

    std::cout << "local_fault_test: all tests passed\n";
    return 0;
}
