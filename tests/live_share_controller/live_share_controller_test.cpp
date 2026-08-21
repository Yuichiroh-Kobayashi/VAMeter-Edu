#include "live_share_controller.h"

#include <cstdlib>
#include <iostream>

namespace LSC = LIVE_SHARE_CONTROLLER;
namespace LSS = LIVE_SHARE_SESSION;

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

    LSC::InputSnapshot MakeInput(bool sideClicked = false,
                                bool sideHeld = false,
                                bool encoderClicked = false,
                                bool encoderHeld = false)
    {
        LSC::InputSnapshot input;
        input.sideClicked = sideClicked;
        input.sideHeld = sideHeld;
        input.encoderClicked = encoderClicked;
        input.encoderHeld = encoderHeld;
        return input;
    }

    // T9. Requirement #7: FaultLatched must block StartRecording and
    // StartLiveView, the same way BusyCleanup already blocks them via the
    // existing Idle-only gate -- no logic change was made to
    // CanStartRecording/CanStartLiveView, only a new enumerator was added
    // upstream; this test pins that the generic `== Idle` gate still
    // covers it.
    void TestCanStartRecording_BlocksFaultLatched()
    {
        Require(!LSC::CanStartRecording(LSC::RecorderActivity::FaultLatched, LSS::State::Inactive),
                "FaultLatched must not permit StartRecording");
        Require(LSC::CanStartRecording(LSC::RecorderActivity::Idle, LSS::State::Inactive),
                "Idle + Inactive share state must still permit StartRecording (regression)");
    }

    // T10.
    void TestCanStartLiveView_BlocksFaultLatched()
    {
        Require(!LSC::CanStartLiveView(LSC::RecorderActivity::FaultLatched, LSS::State::Inactive),
                "FaultLatched must not permit StartLiveView");
        Require(LSC::CanStartLiveView(LSC::RecorderActivity::Idle, LSS::State::Inactive),
                "Idle + Inactive share state must still permit StartLiveView (regression)");
    }

    // Requirement #12: FaultLatched must never resolve to
    // RetryRecorderCleanup -- that gate is BusyCleanup-only. Encoder click
    // while FaultLatched must not be confused with the cleanup-retry path.
    // T1: FaultLatched + SideClick -> remains latched (no StartLiveView).
    void TestResolveWaveformInput_FaultLatchedSideClickIsNone()
    {
        const LSC::ForegroundAction action = LSC::ResolveWaveformInput(
            MakeInput(true, false, false, false), LSC::RecorderActivity::FaultLatched, LSS::State::Inactive, true);
        Require(action == LSC::ForegroundAction::None, "FaultLatched + side click must not resolve to StartLiveView");
    }

    // T2: FaultLatched + SideHold -> remains latched, no Exit/quit. Side hold
    // is F0-L's common Exit affordance elsewhere, but F0-L authority requires
    // Fault to own all input while latched: Side hold must never bypass the
    // ACK contract via the ordinary Exit route. This corrects a prior
    // (incorrect) test that pinned SideHold -> Exit as intended behavior.
    void TestResolveWaveformInput_FaultLatchedSideHoldRemainsLatched()
    {
        const LSC::ForegroundAction action = LSC::ResolveWaveformInput(
            MakeInput(false, true, false, false), LSC::RecorderActivity::FaultLatched, LSS::State::Inactive, true);
        Require(action == LSC::ForegroundAction::None,
                "FaultLatched + side hold must resolve to None, not Exit (Side hold must not bypass the fault latch)");
    }

    // T3: FaultLatched + EncoderHold (no click, no side input) -> remains
    // latched. EncoderHold alone must never resolve to any action.
    void TestResolveWaveformInput_FaultLatchedEncoderHoldIsNone()
    {
        const LSC::ForegroundAction action = LSC::ResolveWaveformInput(
            MakeInput(false, false, false, true), LSC::RecorderActivity::FaultLatched, LSS::State::Inactive, true);
        Require(action == LSC::ForegroundAction::None, "FaultLatched + encoder hold alone must resolve to None");
    }

    // T4/T9/T10 (controller layer): FaultLatched + EncoderClick must resolve
    // to None, never RetryRecorderCleanup or StartRecording. The ACK
    // decision itself belongs to LOCAL_FAULT::CanAcknowledge (see
    // tests/local_fault); this only pins that the controller-level action
    // resolver never routes a Fault-latched encoder click into recorder
    // start or cleanup-retry.
    void TestResolveWaveformInput_FaultLatchedEncoderClickIsNone()
    {
        const LSC::ForegroundAction action = LSC::ResolveWaveformInput(
            MakeInput(false, false, true, false), LSC::RecorderActivity::FaultLatched, LSS::State::Inactive, true);
        Require(action == LSC::ForegroundAction::None,
                "FaultLatched + encoder click must resolve to None, never RetryRecorderCleanup or StartRecording");
    }

    // Regression: BusyCleanup's existing encoder-click => RetryRecorderCleanup
    // routing must remain unaffected by the new FaultLatched enumerator.
    void TestResolveWaveformInput_BusyCleanupStillRoutesRetry()
    {
        const LSC::ForegroundAction action = LSC::ResolveWaveformInput(
            MakeInput(false, false, true, false), LSC::RecorderActivity::BusyCleanup, LSS::State::Inactive, true);
        Require(action == LSC::ForegroundAction::RetryRecorderCleanup,
                "BusyCleanup + encoder click must still resolve to RetryRecorderCleanup (regression)");
    }

    // Requirement #16 (partial, controller-level): live-share activity
    // gates every RecorderActivity value identically regardless of
    // FaultLatched's addition -- a non-Inactive live-share state still
    // forces None for every input.
    void TestResolveWaveformInput_LiveShareActiveBlocksFaultLatchedToo()
    {
        const LSC::ForegroundAction action = LSC::ResolveWaveformInput(
            MakeInput(false, false, true, false), LSC::RecorderActivity::FaultLatched, LSS::State::WifiQr, true);
        Require(action == LSC::ForegroundAction::None,
                "non-Inactive live-share state must force None even for FaultLatched");
    }
} // namespace

int main()
{
    TestCanStartRecording_BlocksFaultLatched();
    TestCanStartLiveView_BlocksFaultLatched();
    TestResolveWaveformInput_FaultLatchedSideClickIsNone();
    TestResolveWaveformInput_FaultLatchedSideHoldRemainsLatched();
    TestResolveWaveformInput_FaultLatchedEncoderHoldIsNone();
    TestResolveWaveformInput_FaultLatchedEncoderClickIsNone();
    TestResolveWaveformInput_BusyCleanupStillRoutesRetry();
    TestResolveWaveformInput_LiveShareActiveBlocksFaultLatchedToo();

    std::cout << "live_share_controller_test: all tests passed\n";
    return 0;
}
