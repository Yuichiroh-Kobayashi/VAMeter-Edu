#include "d2b_stream_start_disposition.h"
#include "d2b_transport_runtime_state.h"
#include "network_web_server_recovery.h"
#include "web_server_owner.h"
#include "web_server_results.h"
#include "web_server_transaction.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace
{
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    struct SideEffects
    {
        unsigned apStarts = 0;
        unsigned apStops = 0;
        unsigned allocations = 0;
        unsigned listens = 0;
        unsigned selectionPublishes = 0;
    };

    D2B::PublicStreamingSnapshot StreamingSnapshot()
    {
        const D2B::PublicStreamingSnapshot snapshot = {
            true,
            true,
            11,
            static_cast<std::uintptr_t>(0x8000),
            3,
            true,
            true,
            true,
            false,
            false,
            11,
            true,
            11,
            true,
            static_cast<std::uintptr_t>(0x8000),
            3,
        };
        return snapshot;
    }

    struct FakeTransaction
    {
        WEB_SERVER_OWNER::State state;
        bool wrapperPresent = true;
        bool stopSucceeds = false;
        unsigned stopCalls = 0;
        unsigned beforeCalls = 0;
        unsigned clearCalls = 0;
        unsigned afterCalls = 0;
        unsigned failureCalls = 0;
        bool afterSawWrapperAbsent = false;
        bool afterSawOwnerSystem = false;
        unsigned eventCount = 0;
        char events[8] = {};

        void event(char value)
        {
            if (eventCount < sizeof(events))
                events[eventCount++] = value;
        }

        static void Before(void* context, std::uintptr_t)
        {
            FakeTransaction* fake = static_cast<FakeTransaction*>(context);
            ++fake->beforeCalls;
            fake->event('b');
        }

        static bool Stop(void* context, std::uintptr_t)
        {
            FakeTransaction* fake = static_cast<FakeTransaction*>(context);
            ++fake->stopCalls;
            fake->event('s');
            return fake->stopSucceeds;
        }

        static void Clear(void* context, std::uintptr_t)
        {
            FakeTransaction* fake = static_cast<FakeTransaction*>(context);
            ++fake->clearCalls;
            fake->wrapperPresent = false;
            fake->event('c');
        }

        static void After(void* context, std::uintptr_t)
        {
            FakeTransaction* fake = static_cast<FakeTransaction*>(context);
            ++fake->afterCalls;
            fake->afterSawWrapperAbsent = !fake->wrapperPresent;
            fake->afterSawOwnerSystem = fake->state.owner() == WEB_SERVER_OWNER::Owner::System;
            fake->event('a');
        }

        static void Failure(void* context, std::uintptr_t)
        {
            FakeTransaction* fake = static_cast<FakeTransaction*>(context);
            ++fake->failureCalls;
            fake->event('f');
        }
    };

    WEB_SERVER_OWNER::TransactionRequest MakeStopRequest(FakeTransaction& fake,
                                                          WEB_SERVER_OWNER::Owner owner,
                                                          std::uintptr_t wrapperKey,
                                                          std::uintptr_t rawHandleKey)
    {
        const WEB_SERVER_OWNER::TransactionRequest request = {
            &fake.state,
            owner,
            wrapperKey,
            rawHandleKey,
            &fake,
            FakeTransaction::Before,
            FakeTransaction::Stop,
            FakeTransaction::Clear,
            FakeTransaction::After,
            FakeTransaction::Failure,
        };
        return request;
    }

    void TestProductionStopTransactionCallbacks()
    {
        FakeTransaction fake;
        Expect(fake.state.acquire(WEB_SERVER_OWNER::Owner::System),
               "transaction fake acquires system owner");
        const std::uintptr_t wrapperKey = 0x100;
        const std::uintptr_t rawHandleKey = 0x200;
        unsigned apStops = 0;
        WEB_SERVER_OWNER::TransactionRequest request =
            MakeStopRequest(fake, WEB_SERVER_OWNER::Owner::System, wrapperKey, rawHandleKey);

        const WEB_SERVER_OWNER::StopTransactionOutcome failed = WEB_SERVER_OWNER::StopOwned(request);
        Expect(failed.result == WEB_SERVER_OWNER::StopResult::RetryRequired && failed.wrapperKey == wrapperKey,
               "production stop transaction retains wrapper after callback failure");
        Expect(fake.stopCalls == 1 && fake.beforeCalls == 1 && fake.afterCalls == 0 && fake.failureCalls == 1 &&
                   fake.eventCount == 3 && fake.events[0] == 'b' && fake.events[1] == 's' && fake.events[2] == 'f',
               "failed stop callback order is before, stop, failure");
        Expect(fake.state.owner() == WEB_SERVER_OWNER::Owner::System && fake.state.retained(),
               "failed stop transaction retains owner and marks recovery");
        Expect(NETWORK_WEB_SERVER_RECOVERY::KeepRecovery(failed.result) &&
                   !NETWORK_WEB_SERVER_RECOVERY::StopCompleted(failed.result),
               "typed UI recovery keeps failed transaction active");
        Expect(!WEB_SERVER_OWNER::ShouldStopApAfterStartFailure(WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry),
               "failed stop transaction keeps AP policy active");
        Expect(apStops == 0, "failed transaction does not stop AP");

        fake.stopSucceeds = true;
        fake.eventCount = 0;
        const WEB_SERVER_OWNER::StopTransactionOutcome succeeded = WEB_SERVER_OWNER::StopOwned(request);
        Expect(succeeded.result == WEB_SERVER_OWNER::StopResult::Stopped && succeeded.wrapperKey == 0,
               "retry success clears wrapper key and returns stopped");
        Expect(fake.stopCalls == 2 && fake.beforeCalls == 2 && fake.afterCalls == 1 && fake.failureCalls == 1 &&
                   fake.clearCalls == 1 && fake.eventCount == 4 && fake.events[0] == 'b' && fake.events[1] == 's' &&
                   fake.events[2] == 'c' && fake.events[3] == 'a' && fake.afterSawWrapperAbsent &&
                   fake.afterSawOwnerSystem,
               "successful retry clears wrapper before lifecycle callback and owner release");
        Expect(fake.state.owner() == WEB_SERVER_OWNER::Owner::None && !fake.state.retained(),
               "successful retry releases owner after lifecycle callback");
        Expect(WEB_SERVER_OWNER::IsStopSuccessful(succeeded.result) && succeeded.wrapperKey == 0 &&
                   fake.state.owner() == WEB_SERVER_OWNER::Owner::None,
               "production AP policy may stop only after zero wrapper and empty owner");
        if (WEB_SERVER_OWNER::IsStopSuccessful(succeeded.result) && succeeded.wrapperKey == 0 &&
            fake.state.owner() == WEB_SERVER_OWNER::Owner::None)
            ++apStops;
        Expect(apStops == 1, "successful retry stops AP once after owner release");

        Expect(WEB_SERVER_OWNER::StartPreflight(fake.state, WEB_SERVER_OWNER::Owner::System, false) ==
                   WEB_SERVER_OWNER::StartPreflightResult::Proceed,
               "transaction success permits next preflight");
        Expect(fake.state.acquire(WEB_SERVER_OWNER::Owner::System), "transaction success permits next acquire");

        fake.stopSucceeds = false;
        fake.wrapperPresent = true;
        const std::uintptr_t partialWrapperKey = 0x300;
        const std::uintptr_t partialRawHandleKey = 0x400;
        const WEB_SERVER_OWNER::TransactionRequest partialRequest = {
            &fake.state,
            WEB_SERVER_OWNER::Owner::System,
            partialWrapperKey,
            partialRawHandleKey,
            &fake,
            nullptr,
            FakeTransaction::Stop,
            nullptr,
            nullptr,
            FakeTransaction::Failure,
        };
        const WEB_SERVER_OWNER::PartialCleanupOutcome partialFailed =
            WEB_SERVER_OWNER::CleanupPartial(partialRequest);
        Expect(partialFailed.result == WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry &&
                   partialFailed.wrapperKey == partialWrapperKey && fake.state.owner() == WEB_SERVER_OWNER::Owner::System &&
                   fake.state.retained() && fake.wrapperPresent,
               "partial non-null handle cleanup failure retains wrapper and owner");
        fake.stopSucceeds = true;
        fake.wrapperPresent = true;
        fake.eventCount = 0;
        const WEB_SERVER_OWNER::StopTransactionOutcome partialRetry =
            WEB_SERVER_OWNER::StopOwned(MakeStopRequest(fake, WEB_SERVER_OWNER::Owner::System, partialWrapperKey,
                                                        partialRawHandleKey));
        Expect(partialRetry.result == WEB_SERVER_OWNER::StopResult::Stopped && partialRetry.wrapperKey == 0 &&
                   !fake.wrapperPresent && fake.state.owner() == WEB_SERVER_OWNER::Owner::None &&
                   fake.eventCount == 4 && fake.events[0] == 'b' && fake.events[1] == 's' && fake.events[2] == 'c' &&
                   fake.events[3] == 'a' && fake.afterSawWrapperAbsent && fake.afterSawOwnerSystem,
               "partial cleanup failure retries through production stop transaction and clears before release");
        if (partialRetry.result == WEB_SERVER_OWNER::StopResult::Stopped && partialRetry.wrapperKey == 0 &&
            fake.state.owner() == WEB_SERVER_OWNER::Owner::None)
            ++apStops;
        Expect(apStops == 2, "partial cleanup recovery permits AP cleanup once");

        FakeTransaction noHandle;
        Expect(noHandle.state.acquire(WEB_SERVER_OWNER::Owner::System),
               "no-wrapper allocation failure owns system before cleanup");
        const WEB_SERVER_OWNER::TransactionRequest noHandleRequest = {
            &noHandle.state,
            WEB_SERVER_OWNER::Owner::System,
            0,
            0,
            &noHandle,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
        };
        const WEB_SERVER_OWNER::PartialCleanupOutcome noHandleCleanup =
            WEB_SERVER_OWNER::CleanupPartial(noHandleRequest);
        Expect(noHandleCleanup.result == WEB_SERVER_OWNER::StartResult::AllocationOrListenFailure &&
                   noHandleCleanup.wrapperKey == 0 && noHandle.state.owner() == WEB_SERVER_OWNER::Owner::None &&
                   WEB_SERVER_OWNER::ShouldStopApAfterStartFailure(noHandleCleanup.result),
               "no-wrapper allocation failure releases owner and permits AP cleanup");
        if (noHandleCleanup.result == WEB_SERVER_OWNER::StartResult::AllocationOrListenFailure &&
            noHandleCleanup.wrapperKey == 0 && noHandle.state.owner() == WEB_SERVER_OWNER::Owner::None)
            ++apStops;
        Expect(apStops == 3, "no-wrapper allocation failure permits AP cleanup once");

        FakeTransaction wrongOwner;
        Expect(wrongOwner.state.acquire(WEB_SERVER_OWNER::Owner::Download), "wrong-owner fake acquires download");
        const WEB_SERVER_OWNER::TransactionRequest wrongRequest =
            MakeStopRequest(wrongOwner, WEB_SERVER_OWNER::Owner::System, 0x500, 0x600);
        const WEB_SERVER_OWNER::StopTransactionOutcome rejected = WEB_SERVER_OWNER::StopOwned(wrongRequest);
        Expect(rejected.result == WEB_SERVER_OWNER::StopResult::RejectedWrongOwner && rejected.wrapperKey == 0x500 &&
                   wrongOwner.stopCalls == 0 && wrongOwner.beforeCalls == 0 && wrongOwner.afterCalls == 0 &&
                   wrongOwner.clearCalls == 0 &&
                   wrongOwner.state.owner() == WEB_SERVER_OWNER::Owner::Download,
               "wrong owner rejects without invoking callbacks or changing state");

        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            FakeTransaction repeated;
            Expect(repeated.state.acquire(WEB_SERVER_OWNER::Owner::System), "repeated transaction owner acquires");
            WEB_SERVER_OWNER::TransactionRequest repeatedRequest =
                MakeStopRequest(repeated, WEB_SERVER_OWNER::Owner::System, 0x700 + cycle, 0x800 + cycle);
            repeated.stopSucceeds = false;
            Expect(WEB_SERVER_OWNER::StopOwned(repeatedRequest).result == WEB_SERVER_OWNER::StopResult::RetryRequired,
                   "repeated transaction failure retains state");
            repeated.stopSucceeds = true;
            const WEB_SERVER_OWNER::StopTransactionOutcome repeatedSuccess =
                WEB_SERVER_OWNER::StopOwned(repeatedRequest);
            Expect(repeatedSuccess.result == WEB_SERVER_OWNER::StopResult::Stopped && repeatedSuccess.wrapperKey == 0 &&
                       repeated.stopCalls == 2 && repeated.clearCalls == 1 && repeated.failureCalls == 1 &&
                       repeated.state.owner() == WEB_SERVER_OWNER::Owner::None,
                   "repeated transaction retry closes cleanly");
        }
    }

    void TestStartStopFailureRecovery()
    {
        WEB_SERVER_OWNER::State state;
        SideEffects effects;
        Expect(state.acquire(WEB_SERVER_OWNER::Owner::System), "system start owns the server");
        ++effects.apStarts;
        Expect(WEB_SERVER_OWNER::StartPreflight(state, WEB_SERVER_OWNER::Owner::System, true) ==
                   WEB_SERVER_OWNER::StartPreflightResult::BusyOtherOwner,
               "a live non-retained system owner is busy, not a stop retry");
        state.markRetained();

        Expect(WEB_SERVER_OWNER::StartPreflight(state,
                                                WEB_SERVER_OWNER::Owner::System,
                                                true) ==
                   WEB_SERVER_OWNER::StartPreflightResult::RetainedServerNeedsStopRetry,
               "stop failure is surfaced as retained start recovery");
        const WEB_SERVER_OWNER::StartResult retained =
            WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry;
        Expect(NETWORK_WEB_SERVER_RECOVERY::DecideStartAction(retained) ==
                   NETWORK_WEB_SERVER_RECOVERY::Action::EnterStopRecovery,
               "retained start enters explicit recovery UI");
        Expect(!WEB_SERVER_OWNER::ShouldStopApAfterStartFailure(retained) && effects.apStops == 0,
               "retained start never stops AP");

        const WEB_SERVER_OWNER::StopResult retryFailure = WEB_SERVER_OWNER::StopResult::RetryRequired;
        Expect(NETWORK_WEB_SERVER_RECOVERY::KeepRecovery(retryFailure), "failed retry stays in recovery UI");
        Expect(!NETWORK_WEB_SERVER_RECOVERY::StopCompleted(retryFailure), "failed retry is not reported stopped");

        Expect(state.release(WEB_SERVER_OWNER::Owner::System), "explicit retry releases retained owner");
        effects.apStops++;
        Expect(NETWORK_WEB_SERVER_RECOVERY::StopCompleted(WEB_SERVER_OWNER::StopResult::Stopped),
               "successful retry completes recovery");
        Expect(state.owner() == WEB_SERVER_OWNER::Owner::None && !state.retained() && effects.apStops == 1,
               "wrapper release precedes AP stop");
        Expect(WEB_SERVER_OWNER::StartPreflight(state, WEB_SERVER_OWNER::Owner::System, false) ==
                   WEB_SERVER_OWNER::StartPreflightResult::Proceed,
               "next clean start is allowed");
        Expect(state.acquire(WEB_SERVER_OWNER::Owner::System), "next clean start succeeds");
    }

    void TestPartialListenAndPreflightNoSideEffects()
    {
        WEB_SERVER_OWNER::State state;
        SideEffects effects;
        Expect(state.acquire(WEB_SERVER_OWNER::Owner::System), "partial listen owns system");
        state.markRetained();
        const unsigned beforeAllocation = effects.allocations;
        const unsigned beforeListen = effects.listens;
        const unsigned beforeApStart = effects.apStarts;
        const unsigned beforeApStop = effects.apStops;
        const unsigned beforeSelection = effects.selectionPublishes;
        Expect(WEB_SERVER_OWNER::StartPreflight(state, WEB_SERVER_OWNER::Owner::System, true) ==
                   WEB_SERVER_OWNER::StartPreflightResult::RetainedServerNeedsStopRetry,
               "partial listen cleanup failure retains server");
        Expect(effects.allocations == beforeAllocation && effects.listens == beforeListen &&
                   effects.apStarts == beforeApStart && effects.apStops == beforeApStop &&
                   effects.selectionPublishes == beforeSelection,
               "retained preflight has zero allocation/listen/AP/selection side effects");

        WEB_SERVER_OWNER::State download;
        Expect(download.acquire(WEB_SERVER_OWNER::Owner::Download), "download owner starts independently");
        const unsigned beforeDownloadSelection = effects.selectionPublishes;
        Expect(WEB_SERVER_OWNER::StartPreflight(download, WEB_SERVER_OWNER::Owner::System, false) ==
                   WEB_SERVER_OWNER::StartPreflightResult::BusyOtherOwner,
               "system cannot displace download owner");
        Expect(effects.selectionPublishes == beforeDownloadSelection && effects.apStops == beforeApStop,
               "busy owner does not clear download selection or stop AP");

        WEB_SERVER_OWNER::State empty;
        Expect(WEB_SERVER_OWNER::StartPreflight(empty, WEB_SERVER_OWNER::Owner::Download, false) ==
                   WEB_SERVER_OWNER::StartPreflightResult::Proceed,
               "empty owner permits download preflight before selection publish");
        ++effects.selectionPublishes;
        Expect(effects.selectionPublishes == beforeDownloadSelection + 1,
               "selection changes only after a successful preflight");

        WEB_SERVER_OWNER::State inconsistent;
        Expect(WEB_SERVER_OWNER::StartPreflight(inconsistent, WEB_SERVER_OWNER::Owner::System, true) ==
                   WEB_SERVER_OWNER::StartPreflightResult::RetainedServerNeedsStopRetry,
               "inconsistent wrapper-without-owner state fails closed as retained recovery");
        Expect(!NETWORK_WEB_SERVER_RECOVERY::StopCompleted(WEB_SERVER_OWNER::StopResult::RetryRequired),
               "inconsistent wrapper state cannot be reported already stopped");
    }

    void TestResultPolicyMatrix()
    {
        using WEB_SERVER_OWNER::StartResult;
        using WEB_SERVER_OWNER::StopResult;
        Expect(NETWORK_WEB_SERVER_RECOVERY::DecideStartAction(StartResult::Started) ==
                   NETWORK_WEB_SERVER_RECOVERY::Action::ContinueWorkflow,
               "started branch continues QR workflow");
        Expect(NETWORK_WEB_SERVER_RECOVERY::DecideStartAction(StartResult::BusyOtherOwner) ==
                   NETWORK_WEB_SERVER_RECOVERY::Action::ShowBusyNotice,
               "busy branch shows a notice");
        Expect(NETWORK_WEB_SERVER_RECOVERY::DecideStartAction(StartResult::AllocationOrListenFailure) ==
                   NETWORK_WEB_SERVER_RECOVERY::Action::ShowStartFailure,
               "allocation/listen branch shows start failure");
        Expect(NETWORK_WEB_SERVER_RECOVERY::DecideStartAction(StartResult::RouteOrRegistrationFailure) ==
                   NETWORK_WEB_SERVER_RECOVERY::Action::ShowStartFailure,
               "registration branch shows route failure");
        Expect(WEB_SERVER_OWNER::ShouldStopApAfterStartFailure(StartResult::AllocationOrListenFailure) &&
                   WEB_SERVER_OWNER::ShouldStopApAfterStartFailure(StartResult::RouteOrRegistrationFailure),
               "non-retained start failures permit AP cleanup");
        Expect(!WEB_SERVER_OWNER::ShouldStopApAfterStartFailure(StartResult::BusyOtherOwner),
               "busy start failure preserves AP");
        const bool releaseSucceeded = false;
        const StartResult allocationReleaseFailure =
            releaseSucceeded ? StartResult::AllocationOrListenFailure : StartResult::RetainedServerNeedsStopRetry;
        Expect(!WEB_SERVER_OWNER::ShouldStopApAfterStartFailure(allocationReleaseFailure),
               "allocation/listen cleanup release failure preserves AP");
        WEB_SERVER_OWNER::State releaseFailureOwner;
        Expect(releaseFailureOwner.acquire(WEB_SERVER_OWNER::Owner::System),
               "release-failure policy model owns system before cleanup");
        releaseFailureOwner.markRetained();
        Expect(WEB_SERVER_OWNER::StartPreflight(releaseFailureOwner,
                                                WEB_SERVER_OWNER::Owner::System,
                                                false) ==
                   WEB_SERVER_OWNER::StartPreflightResult::RetainedServerNeedsStopRetry,
               "failed cleanup release keeps owner in retained recovery");
        Expect(NETWORK_WEB_SERVER_RECOVERY::StopCompleted(StopResult::AlreadyStopped),
               "already-stopped permits AP stop retry");
        Expect(NETWORK_WEB_SERVER_RECOVERY::KeepRecovery(StopResult::RejectedWrongOwner),
               "wrong-owner stop remains recoverable");
        Expect(NETWORK_WEB_SERVER_RECOVERY::KeepRecovery(StopResult::ApStopFailed),
               "AP stop failure remains recoverable");
    }

    void TestWrongOwnerAndGenerationInvariants()
    {
        WEB_SERVER_OWNER::State state;
        Expect(state.acquire(WEB_SERVER_OWNER::Owner::Download), "download owner acquired");
        const std::uint32_t generation = state.generation();
        Expect(!state.release(WEB_SERVER_OWNER::Owner::System), "wrong owner stop is rejected");
        Expect(state.owner() == WEB_SERVER_OWNER::Owner::Download && state.generation() == generation,
               "wrong-owner stop preserves owner and generation");
        Expect(state.release(WEB_SERVER_OWNER::Owner::Download), "download owner releases itself");
        Expect(state.acquire(WEB_SERVER_OWNER::Owner::System) && state.generation() != generation,
               "new owner gets a new generation");

        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            state.markRetained();
            Expect(WEB_SERVER_OWNER::StartPreflight(state, WEB_SERVER_OWNER::Owner::System, true) ==
                       WEB_SERVER_OWNER::StartPreflightResult::RetainedServerNeedsStopRetry,
                   "repeated failure cycle retains the same owner");
            Expect(state.release(WEB_SERVER_OWNER::Owner::System), "retry releases each retained cycle");
            Expect(state.acquire(WEB_SERVER_OWNER::Owner::System), "each cycle can start cleanly");
        }
    }

    void TestStatusAndDisposition()
    {
        const D2B::PublicStreamingSnapshot coherent = StreamingSnapshot();
        Expect(D2B::IsPublicStreaming(coherent), "all production status fields coherent means streaming");
        D2B::PublicStreamingSnapshot stopFailed = coherent;
        stopFailed.lifecycleAccepting = false;
        Expect(!D2B::IsPublicStreaming(stopFailed), "stop-failed lifecycle means idle");
        D2B::PublicStreamingSnapshot pipelineMismatch = coherent;
        pipelineMismatch.pipelineStreamId = 12;
        Expect(!D2B::IsPublicStreaming(pipelineMismatch), "pipeline stream mismatch means idle");
        D2B::PublicStreamingSnapshot handleMismatch = coherent;
        handleMismatch.lifecycleServerHandleKey = static_cast<std::uintptr_t>(0x9000);
        Expect(!D2B::IsPublicStreaming(handleMismatch), "server handle mismatch means idle");
        Expect(D2B::DecideStreamStartDisposition(true) == D2B::StreamStartDisposition::ContinueConnection,
               "successful stream publication keeps connection");
        Expect(D2B::DecideStreamStartDisposition(false) == D2B::StreamStartDisposition::CloseConnection,
               "publication failure closes connection");
    }
} // namespace

int main()
{
    TestProductionStopTransactionCallbacks();
    TestStartStopFailureRecovery();
    TestPartialListenAndPreflightNoSideEffects();
    TestResultPolicyMatrix();
    TestWrongOwnerAndGenerationInvariants();
    TestStatusAndDisposition();
    std::cout << "PASS: D2B stop-failure typed recovery, preflight, status, and disposition helpers\n";
    return 0;
}
