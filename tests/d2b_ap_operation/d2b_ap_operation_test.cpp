#include "network_web_server_recovery.h"
#include "web_server_ap_operation.h"
#include "web_server_owner.h"
#include "web_server_results.h"
#include "web_server_transaction.h"

#include <cstdlib>
#include <iostream>
#include <string>

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

    struct FakeAp
    {
        bool staConnected = false;
        bool disconnectResult = true;
        WEB_SERVER_OWNER::ApModeState mode = WEB_SERVER_OWNER::ApModeState::Disabled;
        WEB_SERVER_OWNER::ApModeState modeAfterStart = WEB_SERVER_OWNER::ApModeState::Enabled;
        WEB_SERVER_OWNER::ApModeState modeAfterStop = WEB_SERVER_OWNER::ApModeState::Disabled;
        bool startResult = true;
        bool stopResult = true;
        bool asynchronousEventBit = false;
        bool updateEventOnStart = true;
        bool updateEventOnStop = true;
        bool eventAfterStart = true;
        bool eventAfterStop = false;
        unsigned staStateCalls = 0;
        unsigned disconnectCalls = 0;
        unsigned modeQueryCalls = 0;
        unsigned startCalls = 0;
        unsigned stopCalls = 0;
        unsigned eventCount = 0;
        char events[64] = {};

        void event(char value)
        {
            if (eventCount < sizeof(events))
                events[eventCount++] = value;
        }

        void resetCalls()
        {
            staStateCalls = 0;
            disconnectCalls = 0;
            modeQueryCalls = 0;
            startCalls = 0;
            stopCalls = 0;
            eventCount = 0;
        }

        static bool IsStaConnected(void* context)
        {
            FakeAp* fake = static_cast<FakeAp*>(context);
            ++fake->staStateCalls;
            fake->event('c');
            return fake->staConnected;
        }

        static bool DisconnectSta(void* context)
        {
            FakeAp* fake = static_cast<FakeAp*>(context);
            ++fake->disconnectCalls;
            fake->event('d');
            return fake->disconnectResult;
        }

        static WEB_SERVER_OWNER::ApModeState QueryApMode(void* context)
        {
            FakeAp* fake = static_cast<FakeAp*>(context);
            ++fake->modeQueryCalls;
            fake->event('q');
            return fake->mode;
        }

        static bool StartAp(void* context)
        {
            FakeAp* fake = static_cast<FakeAp*>(context);
            ++fake->startCalls;
            fake->event('a');
            fake->mode = fake->modeAfterStart;
            if (fake->updateEventOnStart)
                fake->asynchronousEventBit = fake->eventAfterStart;
            return fake->startResult;
        }

        static bool StopAp(void* context)
        {
            FakeAp* fake = static_cast<FakeAp*>(context);
            ++fake->stopCalls;
            fake->event('x');
            if (fake->stopResult)
                fake->mode = fake->modeAfterStop;
            if (fake->updateEventOnStop)
                fake->asynchronousEventBit = fake->eventAfterStop;
            return fake->stopResult;
        }

        WEB_SERVER_OWNER::ApOperationCallbacks callbacks()
        {
            const WEB_SERVER_OWNER::ApOperationCallbacks result = {
                this,
                IsStaConnected,
                DisconnectSta,
                QueryApMode,
                StartAp,
                StopAp,
            };
            return result;
        }
    };

    bool EventsEqual(const FakeAp& fake, const char* expected)
    {
        unsigned expectedLength = 0;
        while (expected[expectedLength] != '\0')
            ++expectedLength;
        if (fake.eventCount != expectedLength)
            return false;
        for (unsigned index = 0; index < expectedLength; ++index)
        {
            if (fake.events[index] != expected[index])
                return false;
        }
        return true;
    }

    void TestApStartSuccess()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "AP start success result");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Active && fake.startCalls == 1 &&
                   fake.modeQueryCalls == 2 && fake.staStateCalls == 1 && fake.disconnectCalls == 0 &&
                   fake.stopCalls == 0 && EventsEqual(fake, "qcaq"),
               "AP start success enters active state and uses synchronous mode");
    }

    void TestStaDisconnectFailure()
    {
        FakeAp fake;
        fake.staConnected = true;
        fake.disconnectResult = false;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StaDisconnectFailed,
               "STA disconnect failure result");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.startCalls == 0 &&
                   fake.modeQueryCalls == 1 && EventsEqual(fake, "qcd"),
               "STA disconnect failure does not attempt AP start");
    }

    void TestApStartFailureCleanupSuccess()
    {
        FakeAp fake;
        fake.startResult = false;
        fake.modeAfterStart = WEB_SERVER_OWNER::ApModeState::Enabled;
        fake.stopResult = true;
        fake.modeAfterStop = WEB_SERVER_OWNER::ApModeState::Disabled;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StartFailed,
               "AP start failure with cleanup success result");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.modeQueryCalls == 3 &&
                   fake.stopCalls == 1 && EventsEqual(fake, "qcaqxq"),
               "AP start failure confirms partial AP and cleans it up successfully");
    }

    void TestApStartFailureRetainsCleanupRetry()
    {
        FakeAp fake;
        fake.startResult = false;
        fake.modeAfterStart = WEB_SERVER_OWNER::ApModeState::Enabled;
        fake.stopResult = false;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired,
               "partial AP start cleanup failure result");
        Expect(operation.stopRetryRequired() && fake.modeQueryCalls == 2 && fake.stopCalls == 1 &&
                   EventsEqual(fake, "qcaqx"),
               "partial AP cleanup failure retains retry state");
    }

    void TestPendingStartReconcilesStaleEnabled()
    {
        FakeAp fake;
        fake.startResult = false;
        fake.modeAfterStart = WEB_SERVER_OWNER::ApModeState::Enabled;
        fake.stopResult = false;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired,
               "setup AP retry state");
        fake.resetCalls();
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired,
               "pending AP start is rejected by synchronous mode");
        Expect(fake.modeQueryCalls == 1 && fake.staStateCalls == 0 && fake.disconnectCalls == 0 &&
                   fake.startCalls == 0 && fake.stopCalls == 0 && EventsEqual(fake, "q"),
               "pending AP start has no side-effect callbacks");
    }

    void TestActiveApStopSuccess()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "active stop setup");
        fake.resetCalls();
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::Stopped,
               "active AP stop success");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.modeQueryCalls == 2 &&
                   fake.stopCalls == 1 && fake.staStateCalls == 0 && fake.disconnectCalls == 0 &&
                   EventsEqual(fake, "qxq"),
               "active AP stop clears state after one bounded stop call");
    }

    void TestApStopFailureRetainsRetry()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "stop failure setup");
        fake.stopResult = false;
        fake.resetCalls();
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed,
               "AP stop failure result");
        Expect(operation.stopRetryRequired() && fake.modeQueryCalls == 1 && fake.stopCalls == 1 &&
                   EventsEqual(fake, "qx"),
               "AP stop failure retains retry state");
    }

    void TestApStopFalseFalseTrueRetry()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "retry sequence setup");
        fake.stopResult = false;
        fake.resetCalls();
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed &&
                   fake.stopCalls == 1,
               "first AP stop retry fails once");
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed &&
                   fake.stopCalls == 2,
               "second AP stop retry fails once");
        fake.stopResult = true;
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::Stopped &&
                   fake.stopCalls == 3,
               "third AP stop retry succeeds once");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive,
               "false false true retry becomes inactive only after synchronous Disabled");
        Expect(fake.modeQueryCalls == 4, "false false true retry mode query count");
    }

    void TestAlreadyStoppedDoesNotTouchSta()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "already-stopped path setup starts AP");
        fake.mode = WEB_SERVER_OWNER::ApModeState::Disabled;
        fake.resetCalls();
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::AlreadyStopped,
               "externally stopped AP is already stopped");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.modeQueryCalls == 1 &&
                   fake.stopCalls == 0 && fake.staStateCalls == 0 && fake.disconnectCalls == 0 &&
                   EventsEqual(fake, "q"),
               "already-stopped path observes synchronous mode without touching STA");
    }

    void TestCallbackOrder()
    {
        FakeAp connected;
        connected.staConnected = true;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(connected.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started &&
                   EventsEqual(connected, "qcdaq"),
               "connected AP start callback order is mode, STA-state, disconnect, AP-start, mode");

        FakeAp failed;
        failed.startResult = false;
        failed.modeAfterStart = WEB_SERVER_OWNER::ApModeState::Enabled;
        failed.stopResult = true;
        WEB_SERVER_OWNER::ApOperation failedOperation;
        Expect(failedOperation.start(failed.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StartFailed &&
                   EventsEqual(failed, "qcaqxq"),
               "failed AP start cleanup callback order is bounded and mode-based");

        FakeAp stopped;
        WEB_SERVER_OWNER::ApOperation stoppedOperation;
        Expect(stoppedOperation.start(stopped.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "stop callback order setup");
        stopped.resetCalls();
        Expect(stoppedOperation.stop(stopped.callbacks()) == WEB_SERVER_OWNER::ApStopResult::Stopped &&
                   EventsEqual(stopped, "qxq"),
               "AP stop callback order is mode, AP-stop, mode");
    }

    void TestApStartFailureUiMapping()
    {
        Expect(NETWORK_WEB_SERVER_RECOVERY::DecideStartAction(WEB_SERVER_OWNER::StartResult::ApStartFailed) ==
                   NETWORK_WEB_SERVER_RECOVERY::Action::ShowStartFailure,
               "ApStartFailed maps to start failure notice");
    }

    void TestRetainedApUiMapping()
    {
        Expect(NETWORK_WEB_SERVER_RECOVERY::DecideStartAction(
                   WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry) ==
                   NETWORK_WEB_SERVER_RECOVERY::Action::EnterStopRecovery,
               "RetainedApNeedsStopRetry maps to stop recovery");
    }

    struct FakeHttpdStop
    {
        bool succeeds;
        unsigned calls;
        static bool Stop(void* context, std::uintptr_t)
        {
            FakeHttpdStop* fake = static_cast<FakeHttpdStop*>(context);
            ++fake->calls;
            return fake->succeeds;
        }
    };

    WEB_SERVER_OWNER::StopTransactionOutcome StopFakeHttpd(WEB_SERVER_OWNER::State& state,
                                                             FakeHttpdStop& fake,
                                                             std::uintptr_t wrapperKey)
    {
        const WEB_SERVER_OWNER::TransactionRequest request = {
            &state,
            WEB_SERVER_OWNER::Owner::System,
            wrapperKey,
            wrapperKey + 1,
            &fake,
            nullptr,
            FakeHttpdStop::Stop,
            nullptr,
            nullptr,
            nullptr,
        };
        return WEB_SERVER_OWNER::StopOwned(request);
    }

    void TestRouteFailureRetainsOnlyApAfterHttpdCleanup()
    {
        FakeAp ap;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(ap.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "route rollback AP setup");
        WEB_SERVER_OWNER::State owner;
        Expect(owner.acquire(WEB_SERVER_OWNER::Owner::System), "route rollback server owner setup");
        FakeHttpdStop httpd = {true, 0};
        Expect(StopFakeHttpd(owner, httpd, 0x100).result == WEB_SERVER_OWNER::StopResult::Stopped &&
                   owner.owner() == WEB_SERVER_OWNER::Owner::None,
               "route rollback HTTPD cleanup succeeds before AP cleanup");
        ap.stopResult = false;
        const WEB_SERVER_OWNER::ApStopResult apResult = operation.stop(ap.callbacks());
        const WEB_SERVER_OWNER::StartResult result = apResult == WEB_SERVER_OWNER::ApStopResult::StopFailed
                                                         ? WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry
                                                         : WEB_SERVER_OWNER::StartResult::RouteOrRegistrationFailure;
        Expect(result == WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry && operation.stopRetryRequired(),
               "route rollback AP cleanup failure retains AP without server owner");
    }

    void TestNormalStopMapsApFailure()
    {
        FakeAp ap;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(ap.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "normal stop AP setup");
        WEB_SERVER_OWNER::State owner;
        Expect(owner.acquire(WEB_SERVER_OWNER::Owner::System), "normal stop server owner setup");
        FakeHttpdStop httpd = {true, 0};
        Expect(StopFakeHttpd(owner, httpd, 0x200).result == WEB_SERVER_OWNER::StopResult::Stopped,
               "normal HTTPD stop succeeds");
        ap.stopResult = false;
        const WEB_SERVER_OWNER::ApStopResult apResult = operation.stop(ap.callbacks());
        const WEB_SERVER_OWNER::StopResult result = apResult == WEB_SERVER_OWNER::ApStopResult::StopFailed
                                                         ? WEB_SERVER_OWNER::StopResult::ApStopFailed
                                                         : WEB_SERVER_OWNER::StopResult::Stopped;
        Expect(result == WEB_SERVER_OWNER::StopResult::ApStopFailed && owner.owner() == WEB_SERVER_OWNER::Owner::None,
               "normal stop propagates AP stop failure");
    }

    void TestAlreadyStoppedHttpdRetriesPendingAp()
    {
        FakeAp ap;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(ap.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "already-stopped AP setup");
        ap.stopResult = false;
        Expect(operation.stop(ap.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed,
               "already-stopped pending AP setup");
        WEB_SERVER_OWNER::State owner;
        FakeHttpdStop httpd = {true, 0};
        const WEB_SERVER_OWNER::TransactionRequest request = {
            &owner,
            WEB_SERVER_OWNER::Owner::System,
            0,
            0,
            &httpd,
            nullptr,
            FakeHttpdStop::Stop,
            nullptr,
            nullptr,
            nullptr,
        };
        const WEB_SERVER_OWNER::StopResult serverResult = WEB_SERVER_OWNER::StopOwned(request).result;
        ap.stopResult = true;
        const WEB_SERVER_OWNER::ApStopResult apResult = operation.stop(ap.callbacks());
        Expect(serverResult == WEB_SERVER_OWNER::StopResult::AlreadyStopped &&
                   WEB_SERVER_OWNER::IsStopSuccessful(serverResult) &&
                   apResult == WEB_SERVER_OWNER::ApStopResult::Stopped &&
                   operation.state() == WEB_SERVER_OWNER::ApState::Inactive,
               "HTTPD already-stopped path retries pending AP and succeeds");
    }

    void TestRetainedApBlocksSystemStart()
    {
        FakeAp ap;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(ap.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "system gate AP setup");
        ap.stopResult = false;
        Expect(operation.stop(ap.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed,
               "system gate AP retry setup");
        ap.resetCalls();
        const bool proceed = operation.reconcileStartPreflight(ap.callbacks());
        const WEB_SERVER_OWNER::StartResult result =
            proceed ? WEB_SERVER_OWNER::StartResult::Started : WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry;
        Expect(result == WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry && ap.modeQueryCalls == 1 &&
                   ap.staStateCalls == 0 && ap.startCalls == 0 && ap.stopCalls == 0,
               "retained AP blocks System start before side effects");
    }

    void TestRetainedApBlocksDownloadSelection()
    {
        FakeAp ap;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(ap.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "download gate AP setup");
        ap.stopResult = false;
        Expect(operation.stop(ap.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed,
               "download gate AP retry setup");
        const std::string existingSelection = "REC-001.csv";
        std::string selection = existingSelection;
        ap.resetCalls();
        const bool started = operation.reconcileStartPreflight(ap.callbacks());
        if (started)
            selection = "REC-002.csv";
        Expect(!started && selection == existingSelection && ap.modeQueryCalls == 1 && ap.startCalls == 0 &&
                   ap.stopCalls == 0,
               "retained AP blocks Download start and preserves selection");
    }

    void TestDelayedApStartCleanup()
    {
        FakeAp fake;
        fake.startResult = true;
        fake.modeAfterStart = WEB_SERVER_OWNER::ApModeState::Disabled;
        fake.asynchronousEventBit = false;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StartFailed,
               "delayed AP_START is reconciled as partial cleanup");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.startCalls == 1 &&
                   fake.stopCalls == 1 && fake.modeQueryCalls == 3 && !fake.asynchronousEventBit,
               "delayed AP_START cleanup is bounded and event-independent");
    }

    void TestDelayedApStop()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "delayed AP_STOP setup");
        fake.modeAfterStop = WEB_SERVER_OWNER::ApModeState::Enabled;
        fake.resetCalls();
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed,
               "delayed AP_STOP does not claim stopped");
        Expect(operation.stopRetryRequired() && fake.stopCalls == 1 && fake.modeQueryCalls == 2,
               "delayed AP_STOP has one bounded stop attempt");
        fake.mode = WEB_SERVER_OWNER::ApModeState::Disabled;
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::AlreadyStopped &&
                   fake.stopCalls == 1,
               "delayed AP_STOP retry observes Disabled without duplicate stop");
    }

    void TestInactiveEnabledStartReject()
    {
        FakeAp fake;
        fake.mode = WEB_SERVER_OWNER::ApModeState::Enabled;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(!operation.reconcileStartPreflight(fake.callbacks()), "Inactive+Enabled preflight rejects");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::StopRetryRequired &&
                   operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired,
               "Inactive+Enabled start remains fail-closed");
        Expect(fake.modeQueryCalls == 2 && fake.startCalls == 0 && fake.stopCalls == 0,
               "Inactive+Enabled start has no AP side effects");
    }

    void TestInactiveEnabledStopCleanup()
    {
        FakeAp fake;
        fake.mode = WEB_SERVER_OWNER::ApModeState::Enabled;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::Stopped,
               "Inactive+Enabled stop performs explicit cleanup");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.stopCalls == 1 &&
                   fake.modeQueryCalls == 2,
               "Inactive+Enabled stop confirms Disabled postcondition");
    }

    void TestUnknownPreStart()
    {
        FakeAp fake;
        fake.mode = WEB_SERVER_OWNER::ApModeState::Unknown;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired,
               "Unknown pre-start is fail-closed");
        Expect(operation.stopRetryRequired() && fake.modeQueryCalls == 1 && fake.startCalls == 0 &&
                   fake.stopCalls == 0,
               "Unknown pre-start has no side effects");
    }

    void TestUnknownStopBoundedAttempt()
    {
        FakeAp fake;
        fake.mode = WEB_SERVER_OWNER::ApModeState::Unknown;
        fake.stopResult = false;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed,
               "Unknown stop attempts one cleanup");
        Expect(operation.stopRetryRequired() && fake.stopCalls == 1 && fake.modeQueryCalls == 1,
               "Unknown stop failure is bounded");
        fake.stopResult = true;
        fake.modeAfterStop = WEB_SERVER_OWNER::ApModeState::Unknown;
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed &&
                   fake.stopCalls == 2 && fake.modeQueryCalls == 3,
               "Unknown stop success callback still requires Disabled postcondition");
    }

    void TestStopTruePostEnabledFailure()
    {
        FakeAp fake;
        fake.mode = WEB_SERVER_OWNER::ApModeState::Enabled;
        fake.modeAfterStop = WEB_SERVER_OWNER::ApModeState::Enabled;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed,
               "stop true with post Enabled fails closed");
        Expect(operation.stopRetryRequired() && fake.stopCalls == 1 && fake.modeQueryCalls == 2,
               "stop true with post Enabled retains retry");
    }

    void TestImmediateStartAfterStopIgnoresStaleEvent()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "immediate start setup");
        fake.modeAfterStop = WEB_SERVER_OWNER::ApModeState::Disabled;
        fake.asynchronousEventBit = true;
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::Stopped,
               "immediate stop setup");
        Expect(fake.asynchronousEventBit == false, "fake event may settle independently");
        fake.asynchronousEventBit = true;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "stale AP event does not block immediate start");
        Expect(fake.startCalls == 2 && fake.stopCalls == 1 && fake.modeQueryCalls == 6,
               "stop then start uses synchronous mode transitions only");
    }

    void TestApstaStopDoesNotDisconnectSta()
    {
        FakeAp fake;
        fake.staConnected = false;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "APSTA stop setup");
        fake.staConnected = true;
        const unsigned staStateCallsBeforeStop = fake.staStateCalls;
        const unsigned disconnectCallsBeforeStop = fake.disconnectCalls;
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::Stopped,
               "APSTA stop succeeds");
        Expect(fake.staStateCalls == staStateCallsBeforeStop && fake.disconnectCalls == disconnectCallsBeforeStop,
               "AP stop never disconnects STA");
    }

    void TestSynchronousModeIgnoresIndependentEventBit()
    {
        FakeAp startedWithStaleEvent;
        startedWithStaleEvent.eventAfterStart = false;
        WEB_SERVER_OWNER::ApOperation startedOperation;
        Expect(startedOperation.start(startedWithStaleEvent.callbacks()) ==
                   WEB_SERVER_OWNER::ApStartResult::Started &&
                   startedWithStaleEvent.mode == WEB_SERVER_OWNER::ApModeState::Enabled &&
                   !startedWithStaleEvent.asynchronousEventBit,
               "start uses Enabled mode even when AP_START event bit is stale false");

        FakeAp failedWithStaleEvent;
        failedWithStaleEvent.startResult = false;
        failedWithStaleEvent.modeAfterStart = WEB_SERVER_OWNER::ApModeState::Enabled;
        failedWithStaleEvent.eventAfterStart = false;
        failedWithStaleEvent.eventAfterStop = false;
        WEB_SERVER_OWNER::ApOperation failedOperation;
        Expect(failedOperation.start(failedWithStaleEvent.callbacks()) ==
                   WEB_SERVER_OWNER::ApStartResult::StartFailed &&
                   failedWithStaleEvent.stopCalls == 1 && !failedWithStaleEvent.asynchronousEventBit,
               "start cleanup uses Enabled mode even when AP_START event bit is stale false");

        FakeAp stoppedWithStaleEvent;
        stoppedWithStaleEvent.eventAfterStop = true;
        WEB_SERVER_OWNER::ApOperation stoppedOperation;
        Expect(stoppedOperation.start(stoppedWithStaleEvent.callbacks()) ==
                   WEB_SERVER_OWNER::ApStartResult::Started &&
                   stoppedOperation.stop(stoppedWithStaleEvent.callbacks()) ==
                       WEB_SERVER_OWNER::ApStopResult::Stopped &&
                   stoppedWithStaleEvent.mode == WEB_SERVER_OWNER::ApModeState::Disabled &&
                   stoppedWithStaleEvent.asynchronousEventBit,
               "stop uses Disabled mode even when AP_STOP event bit is stale true");
    }

    void TestMissingCallbacksAreBounded()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperationCallbacks missingQuery = fake.callbacks();
        missingQuery.queryApMode = nullptr;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(missingQuery) == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired &&
                   fake.startCalls == 0,
               "missing mode query is Unknown before start side effects");

        FakeAp missingStopFake;
        WEB_SERVER_OWNER::ApOperationCallbacks missingStop = missingStopFake.callbacks();
        missingStop.stopAp = nullptr;
        missingStopFake.mode = WEB_SERVER_OWNER::ApModeState::Enabled;
        WEB_SERVER_OWNER::ApOperation missingStopOperation;
        Expect(missingStopOperation.stop(missingStop) == WEB_SERVER_OWNER::ApStopResult::StopFailed &&
                   missingStopFake.stopCalls == 0,
               "missing stop callback is bounded");

        FakeAp missingDisconnectFake;
        WEB_SERVER_OWNER::ApOperationCallbacks missingDisconnect = missingDisconnectFake.callbacks();
        missingDisconnectFake.staConnected = true;
        missingDisconnect.disconnectSta = nullptr;
        WEB_SERVER_OWNER::ApOperation missingDisconnectOperation;
        Expect(missingDisconnectOperation.start(missingDisconnect) ==
                   WEB_SERVER_OWNER::ApStartResult::StaDisconnectFailed &&
                   missingDisconnectFake.startCalls == 0,
               "missing STA disconnect callback is bounded");
    }

    void TestOneHundredCleanCycles()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperation operation;
        for (unsigned cycle = 0; cycle < 100; ++cycle)
        {
            Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
                   "100-cycle start remains clean");
            Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::Stopped,
                   "100-cycle stop remains clean");
        }
        Expect(fake.startCalls == 100 && fake.stopCalls == 100 && fake.staStateCalls == 100 &&
                   fake.disconnectCalls == 0 && fake.modeQueryCalls == 400,
               "100 cycles have exact callback counts");
    }
} // namespace

int main()
{
    TestApStartSuccess();
    TestStaDisconnectFailure();
    TestApStartFailureCleanupSuccess();
    TestApStartFailureRetainsCleanupRetry();
    TestPendingStartReconcilesStaleEnabled();
    TestActiveApStopSuccess();
    TestApStopFailureRetainsRetry();
    TestApStopFalseFalseTrueRetry();
    TestAlreadyStoppedDoesNotTouchSta();
    TestCallbackOrder();
    TestApStartFailureUiMapping();
    TestRetainedApUiMapping();
    TestRouteFailureRetainsOnlyApAfterHttpdCleanup();
    TestNormalStopMapsApFailure();
    TestAlreadyStoppedHttpdRetriesPendingAp();
    TestRetainedApBlocksSystemStart();
    TestRetainedApBlocksDownloadSelection();
    TestDelayedApStartCleanup();
    TestDelayedApStop();
    TestInactiveEnabledStartReject();
    TestInactiveEnabledStopCleanup();
    TestUnknownPreStart();
    TestUnknownStopBoundedAttempt();
    TestStopTruePostEnabledFailure();
    TestImmediateStartAfterStopIgnoresStaleEvent();
    TestApstaStopDoesNotDisconnectSta();
    TestSynchronousModeIgnoresIndependentEventBit();
    TestMissingCallbacksAreBounded();
    TestOneHundredCleanCycles();
    std::cout << "PASS: D2B AP operation reconciliation (29 cases, including 17 regressions)\n";
    return 0;
}
