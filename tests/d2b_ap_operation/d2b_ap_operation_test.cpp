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
        bool apStarted = false;
        bool startResult = true;
        bool stopResult = true;
        unsigned staStateCalls = 0;
        unsigned disconnectCalls = 0;
        unsigned apStateCalls = 0;
        unsigned startCalls = 0;
        unsigned stopCalls = 0;
        unsigned eventCount = 0;
        char events[16] = {};

        void event(char value)
        {
            if (eventCount < sizeof(events))
                events[eventCount++] = value;
        }

        void resetCalls()
        {
            staStateCalls = 0;
            disconnectCalls = 0;
            apStateCalls = 0;
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

        static bool IsApStarted(void* context)
        {
            FakeAp* fake = static_cast<FakeAp*>(context);
            ++fake->apStateCalls;
            fake->event('p');
            return fake->apStarted;
        }

        static bool StartAp(void* context)
        {
            FakeAp* fake = static_cast<FakeAp*>(context);
            ++fake->startCalls;
            fake->event('a');
            if (fake->startResult)
                fake->apStarted = true;
            return fake->startResult;
        }

        static bool StopAp(void* context)
        {
            FakeAp* fake = static_cast<FakeAp*>(context);
            ++fake->stopCalls;
            fake->event('x');
            if (fake->stopResult)
                fake->apStarted = false;
            return fake->stopResult;
        }

        WEB_SERVER_OWNER::ApOperationCallbacks callbacks()
        {
            const WEB_SERVER_OWNER::ApOperationCallbacks result = {
                this,
                IsStaConnected,
                DisconnectSta,
                IsApStarted,
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
                   fake.staStateCalls == 1 && fake.disconnectCalls == 0 && EventsEqual(fake, "ca"),
               "AP start success enters active state and calls callbacks once");
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
                   EventsEqual(fake, "cd"),
               "STA disconnect failure does not attempt AP start");
    }

    void TestApStartFailureCleanupSuccess()
    {
        FakeAp fake;
        fake.startResult = false;
        fake.apStarted = true;
        fake.stopResult = true;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StartFailed,
               "AP start failure with cleanup success result");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.apStateCalls == 1 &&
                   fake.stopCalls == 1 && EventsEqual(fake, "capx"),
               "AP start failure confirms partial AP and cleans it up successfully");
    }

    void TestApStartFailureRetainsCleanupRetry()
    {
        FakeAp fake;
        fake.startResult = false;
        fake.apStarted = true;
        fake.stopResult = false;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired,
               "partial AP start cleanup failure result");
        Expect(operation.stopRetryRequired() && fake.stopCalls == 1 && EventsEqual(fake, "capx"),
               "partial AP cleanup failure retains retry state");
    }

    void TestPendingStartRejectedWithoutCallbacks()
    {
        FakeAp fake;
        fake.startResult = false;
        fake.apStarted = true;
        fake.stopResult = false;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired,
               "setup AP retry state");
        fake.resetCalls();
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired,
               "pending AP start is rejected");
        Expect(fake.eventCount == 0 && fake.staStateCalls == 0 && fake.disconnectCalls == 0 &&
                   fake.startCalls == 0 && fake.stopCalls == 0,
               "pending AP start has no callback side effects");
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
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.apStateCalls == 1 &&
                   fake.stopCalls == 1 && EventsEqual(fake, "px"),
               "active AP stop clears state after one stop call");
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
        Expect(operation.stopRetryRequired() && fake.stopCalls == 1 && EventsEqual(fake, "px"),
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
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed && fake.stopCalls == 1,
               "first AP stop retry fails once");
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::StopFailed && fake.stopCalls == 2,
               "second AP stop retry fails once");
        fake.stopResult = true;
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::Stopped && fake.stopCalls == 3,
               "third AP stop retry succeeds once");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.apStateCalls == 3,
               "false false true retry becomes inactive only after success");
    }

    void TestAlreadyStoppedDoesNotTouchSta()
    {
        FakeAp fake;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(fake.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "already-stopped path setup starts AP");
        fake.apStarted = false;
        fake.resetCalls();
        Expect(operation.stop(fake.callbacks()) == WEB_SERVER_OWNER::ApStopResult::AlreadyStopped,
               "externally stopped AP is already stopped");
        Expect(operation.state() == WEB_SERVER_OWNER::ApState::Inactive && fake.apStateCalls == 1 &&
                   fake.stopCalls == 0 && fake.staStateCalls == 0 && fake.disconnectCalls == 0 &&
                   EventsEqual(fake, "p"),
               "already-stopped path observes AP state once without touching STA");
    }

    void TestCallbackOrder()
    {
        FakeAp connected;
        connected.staConnected = true;
        WEB_SERVER_OWNER::ApOperation operation;
        Expect(operation.start(connected.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started &&
                   EventsEqual(connected, "cda"),
               "connected AP start callback order is STA-state, disconnect, AP-start");

        FakeAp failed;
        failed.startResult = false;
        failed.apStarted = true;
        failed.stopResult = true;
        WEB_SERVER_OWNER::ApOperation failedOperation;
        Expect(failedOperation.start(failed.callbacks()) == WEB_SERVER_OWNER::ApStartResult::StartFailed &&
                   EventsEqual(failed, "capx"),
               "failed AP start cleanup callback order is STA-state, AP-start, AP-state, AP-stop");

        FakeAp stopped;
        WEB_SERVER_OWNER::ApOperation stoppedOperation;
        Expect(stoppedOperation.start(stopped.callbacks()) == WEB_SERVER_OWNER::ApStartResult::Started,
               "stop callback order setup");
        stopped.resetCalls();
        Expect(stoppedOperation.stop(stopped.callbacks()) == WEB_SERVER_OWNER::ApStopResult::Stopped &&
                   EventsEqual(stopped, "px"),
               "AP stop callback order is AP-state, AP-stop");
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
                   WEB_SERVER_OWNER::IsStopSuccessful(serverResult) && apResult == WEB_SERVER_OWNER::ApStopResult::Stopped &&
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
        const WEB_SERVER_OWNER::StartResult result = operation.stopRetryRequired()
                                                          ? WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry
                                                          : WEB_SERVER_OWNER::StartResult::Started;
        Expect(result == WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry && ap.eventCount == 0,
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
        const bool started = !operation.stopRetryRequired();
        if (started)
            selection = "REC-002.csv";
        Expect(!started && selection == existingSelection && ap.eventCount == 0,
               "retained AP blocks Download start and preserves selection");
    }
} // namespace

int main()
{
    TestApStartSuccess();
    TestStaDisconnectFailure();
    TestApStartFailureCleanupSuccess();
    TestApStartFailureRetainsCleanupRetry();
    TestPendingStartRejectedWithoutCallbacks();
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
    std::cout << "PASS: D2B AP operation result propagation (17 cases)\n";
    return 0;
}
