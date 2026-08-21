#include "d2b_httpd_lifecycle_state.h"
#include "d2b_httpd_send_pump_state.h"

#include <cstdlib>
#include <iostream>

namespace
{
    using D2B_HTTPD_LIFECYCLE::BeginStopResult;
    using D2B_HTTPD_LIFECYCLE::Phase;
    using D2B_HTTPD_LIFECYCLE::State;
    using D2B_HTTPD_SEND_PUMP::BeginResult;
    using D2B_HTTPD_SEND_PUMP::FinishResult;
    using D2B_HTTPD_SEND_PUMP::ScheduleResult;

    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    struct Model
    {
        State lifecycle;
        D2B_HTTPD_SEND_PUMP::State pump;
        const std::uintptr_t handle = 0x1000;
        std::uint32_t generation = 7;
        bool ownerOpen = false;
        bool streamActive = false;
        bool producerActive = false;
        bool sessionPublished = false;
        bool sessionClosed = true;
        std::uint32_t activeStreamId = 0;
        std::uint32_t nextStreamId = 1;
        bool mutexHeld = false;
        bool stopWaiting = false;
        bool closeWaiting = false;
        unsigned queuedOutputFrames = 0;
        unsigned queueWorkCalls = 0;
        unsigned stoppedHandleQueueWorkCalls = 0;
        unsigned sendAttempts = 0;
        unsigned stoppedHandleSendAttempts = 0;
        unsigned sends = 0;
        unsigned transportFieldWrites = 0;

        void start()
        {
            Expect(lifecycle.beginRunning(handle, generation), "model server start commits after routes");
        }

        bool open()
        {
            if (mutexHeld)
                return false;
            mutexHeld = true;
            const bool accepted = lifecycle.accepting(handle, generation) && !ownerOpen;
            if (accepted)
            {
                ownerOpen = pump.activate(generation);
                sessionClosed = !ownerOpen;
            }
            mutexHeld = false;
            return accepted && ownerOpen;
        }

        bool startStream(bool publish)
        {
            if (mutexHeld)
                return false;
            mutexHeld = true;
            const bool accepted = ownerOpen && lifecycle.accepting(handle, generation) && !streamActive;
            if (!accepted)
            {
                mutexHeld = false;
                return false;
            }
            streamActive = true;
            producerActive = true;
            activeStreamId = nextStreamId++;
            if (publish)
                sessionPublished = true;
            else
            {
                producerActive = false;
                streamActive = false;
                activeStreamId = 0;
                queuedOutputFrames = 0;
                mutexHeld = false;
                return false;
            }
            mutexHeld = false;
            return true;
        }

        bool requestPreStop()
        {
            if (mutexHeld)
            {
                stopWaiting = true;
                return false;
            }
            preStop();
            return true;
        }

        D2B_HTTPD_SEND_PUMP::ScheduleDecision queueWork()
        {
            const D2B_HTTPD_SEND_PUMP::ScheduleDecision inactive = {ScheduleResult::Inactive, 0};
            if (mutexHeld || !ownerOpen || !streamActive)
                return inactive;
            const D2B_HTTPD_SEND_PUMP::ScheduleDecision decision = pump.request(generation);
            if (decision.result == ScheduleResult::Accepted)
            {
                if (lifecycle.accepting(handle, generation))
                    ++queueWorkCalls;
                else
                    ++stoppedHandleQueueWorkCalls;
            }
            return decision;
        }

        bool orderlyStop()
        {
            if (mutexHeld || !ownerOpen || !streamActive || !producerActive || !sessionPublished)
                return false;
            const D2B_HTTPD_SEND_PUMP::ScheduleDecision scheduled = queueWork();
            if (scheduled.result != ScheduleResult::Accepted)
                return false;

            mutexHeld = true;
            if (pump.begin(generation, scheduled.token) != BeginResult::Began)
            {
                mutexHeld = false;
                return false;
            }
            ++sendAttempts;
            ++sends;
            const D2B_HTTPD_SEND_PUMP::FinishDecision finished =
                pump.finishOrderlyStream(generation, scheduled.token);
            const bool completed = finished.result == FinishResult::Idle;
            if (completed)
            {
                producerActive = false;
                streamActive = false;
                sessionPublished = false;
                activeStreamId = 0;
                queuedOutputFrames = 0;
            }
            mutexHeld = false;
            return completed;
        }

        bool sendFrame(bool validSocket, bool sendSucceeds)
        {
            if (mutexHeld)
                return false;
            if (!ownerOpen || !lifecycle.accepting(handle, generation))
                return false;
            mutexHeld = true;
            ++sendAttempts;
            const bool sent = validSocket && sendSucceeds;
            if (sent)
            {
                ++sends;
                if (queuedOutputFrames != 0)
                    --queuedOutputFrames;
            }
            mutexHeld = false;
            return sent;
        }

        bool requestCloseCallback()
        {
            if (mutexHeld)
            {
                closeWaiting = true;
                return false;
            }
            closeCallback();
            return true;
        }

        void closeCallback()
        {
            mutexHeld = true;
            closeWaiting = false;
            (void)pump.invalidate(generation);
            producerActive = false;
            streamActive = false;
            ownerOpen = false;
            sessionPublished = false;
            sessionClosed = true;
            activeStreamId = 0;
            queuedOutputFrames = 0;
            mutexHeld = false;
        }

        void preStop()
        {
            mutexHeld = true;
            stopWaiting = false;
            const BeginStopResult result = lifecycle.beginStop(handle, generation);
            Expect(result == BeginStopResult::First || result == BeginStopResult::Retry ||
                       result == BeginStopResult::Repeated || result == BeginStopResult::Inactive,
                   "pre-stop accepts first, retry, repeated, and already-stopped calls");
            (void)pump.invalidate(generation);
            producerActive = false;
            streamActive = false;
            ownerOpen = false;
            activeStreamId = 0;
            queuedOutputFrames = 0;
            mutexHeld = false;
        }

        void postStop(bool success)
        {
            mutexHeld = true;
            if (success)
                (void)lifecycle.stopSuccess(handle, generation);
            else
                (void)lifecycle.stopFailure(handle, generation);
            mutexHeld = false;
            if (success)
            {
                sessionPublished = false;
                sessionClosed = true;
                ownerOpen = false;
                streamActive = false;
                producerActive = false;
                activeStreamId = 0;
                queuedOutputFrames = 0;
                transportFieldWrites = 0;
            }
        }

        bool completed() const
        {
            const D2B_HTTPD_LIFECYCLE::Snapshot server = lifecycle.snapshot();
            const D2B_HTTPD_SEND_PUMP::Snapshot work = pump.snapshot();
            return server.phase == Phase::Stopped && server.handleKey == 0 && server.generation == 0 &&
                   !ownerOpen && !streamActive && !producerActive && !sessionPublished && sessionClosed &&
                   activeStreamId == 0 && queuedOutputFrames == 0 && !mutexHeld && !work.active && !work.pending &&
                   !work.executing &&
                   stoppedHandleQueueWorkCalls == 0 && stoppedHandleSendAttempts == 0;
        }

        bool invariant() const
        {
            return !producerActive || (ownerOpen && streamActive && sessionPublished && activeStreamId != 0);
        }
    };

    void TestReceivePreStopOwnership()
    {
        Model model;
        model.start();
        Expect(model.open(), "HTTPD receive opens owner");
        model.transportFieldWrites = 4;
        model.preStop();
        Expect(model.transportFieldWrites == 4, "application pre-stop does not mutate Transport fields");
        Expect(!model.producerActive && !model.ownerOpen, "pre-stop clears producer and pipeline only");
    }

    void TestStartTransactionStopWins()
    {
        Model model;
        model.start();
        Expect(model.open(), "transaction model opens owner");
        model.preStop();
        Expect(!model.startStream(false), "stop wins before StartStream transaction publication");
        Expect(!model.sessionPublished && model.invariant(), "rejected transaction publishes no session");
    }

    void TestStartCloseSerialization()
    {
        Model model;
        model.start();
        Expect(model.open(), "serialized model opens owner");
        Expect(model.startStream(true), "serialized StartStream completes before close");
        Expect(model.invariant(), "producer active implies owner, stream, and session publication");
        model.preStop();
        Expect(!model.producerActive && !model.streamActive && !model.ownerOpen,
               "close after StartStream clears all active state");
    }

    void TestOrderlyStreamReuse()
    {
        Model model;
        model.start();
        Expect(model.open(), "same WebSocket opens once for orderly stream reuse");
        Expect(model.startStream(true), "first stream starts on connection generation");
        const std::uint32_t firstStreamId = model.activeStreamId;
        Expect(model.orderlyStop() && model.invariant() && !model.mutexHeld,
               "first orderly stop finishes producer/pipeline/session stream state");
        const D2B_HTTPD_SEND_PUMP::Snapshot firstIdle = model.pump.snapshot();
        Expect(model.ownerOpen && !model.sessionClosed && !model.streamActive && !model.producerActive &&
                   firstIdle.active && !firstIdle.pending && !firstIdle.executing && model.queueWorkCalls == 1 &&
                   model.sends == 1,
               "first orderly stop leaves active idle connection pump and one queue/send");

        Expect(model.startStream(true) && model.activeStreamId != firstStreamId && model.invariant() &&
                   !model.mutexHeld,
               "second stream reuses same owner and gets a new stream id");
        Expect(model.orderlyStop() && model.invariant() && !model.mutexHeld,
               "second orderly stop completes on same WebSocket");
        const D2B_HTTPD_SEND_PUMP::Snapshot secondIdle = model.pump.snapshot();
        Expect(secondIdle.active && !secondIdle.pending && !secondIdle.executing && model.queueWorkCalls == 2 &&
                   model.sends == 2,
               "second stream schedules and sends through reusable idle pump");

        Model publicationFailure;
        publicationFailure.start();
        Expect(publicationFailure.open(), "publication failure retry opens owner");
        Expect(!publicationFailure.startStream(false) && publicationFailure.invariant() &&
                   !publicationFailure.mutexHeld,
               "publication failure rolls back producer before handler closes connection");
        publicationFailure.closeCallback();
        const D2B_HTTPD_SEND_PUMP::Snapshot closedAfterFailure = publicationFailure.pump.snapshot();
        Expect(!closedAfterFailure.active && !closedAfterFailure.pending && !closedAfterFailure.executing,
               "publication failure closes and invalidates the connection pump");
        Expect(publicationFailure.open() && publicationFailure.startStream(true) &&
                   publicationFailure.activeStreamId != 1 && publicationFailure.orderlyStop() &&
                   publicationFailure.invariant() && !publicationFailure.mutexHeld &&
                   publicationFailure.queueWorkCalls == 1,
               "reconnect obtains a new stream id after publication failure");

        model.closeCallback();
        const D2B_HTTPD_SEND_PUMP::ScheduleDecision closeQueue = model.queueWork();
        Expect(!model.pump.snapshot().active && closeQueue.result == ScheduleResult::Inactive &&
                   model.pump.request(model.generation).result == ScheduleResult::Inactive &&
                   model.pump.begin(model.generation, 1) == BeginResult::Stale &&
                   model.invariant() && !model.mutexHeld,
               "close after orderly stream invalidates pump and rejects stale callback/queue");

        model.preStop();
        const D2B_HTTPD_SEND_PUMP::ScheduleDecision stoppedQueue = model.queueWork();
        Expect(stoppedQueue.token == 0 && stoppedQueue.result == ScheduleResult::Inactive &&
                   !model.pump.snapshot().active && model.pump.request(model.generation).result == ScheduleResult::Inactive &&
                   model.invariant() && !model.mutexHeld,
               "server stop invalidates connection pump and rejects stale queue_work");
        model.postStop(true);
        Expect(model.completed(), "orderly reuse server stop reaches complete snapshot");
    }

    void TestPendingPumpStop()
    {
        Model model;
        model.start();
        Expect(model.open() && model.startStream(true), "pending pump stream starts");
        const D2B_HTTPD_SEND_PUMP::ScheduleDecision pending = model.pump.request(model.generation);
        Expect(pending.result == ScheduleResult::Accepted, "pending pump queues exactly one callback");
        model.preStop();
        Expect(model.pump.begin(model.generation, pending.token) == BeginResult::Stale,
               "pending callback is stale after pre-stop");
        Expect(!model.pump.snapshot().pending && !model.pump.snapshot().executing,
               "pending pump ownership is invalidated");
    }

    void TestExecutingPumpStop()
    {
        Model model;
        model.start();
        Expect(model.open() && model.startStream(true), "executing pump stream starts");
        const std::uintptr_t token = model.pump.request(model.generation).token;
        Expect(model.pump.begin(token) == BeginResult::Began, "pump callback begins");
        model.mutexHeld = true;
        Expect(!model.requestPreStop() && model.stopWaiting,
               "stop callback waits while the executing send owns the mutex");
        ++model.sends;
        const D2B_HTTPD_SEND_PUMP::FinishDecision callback = model.pump.finish(model.generation, token, true);
        Expect(callback.result == FinishResult::Rescheduled, "executing callback releases and reschedules work");
        model.mutexHeld = false;
        Expect(model.requestPreStop(), "waiting stop acquires the mutex after callback release");
        const unsigned sendsBefore = model.sends;
        Expect(model.pump.begin(model.generation, callback.token) == BeginResult::Stale,
               "stop invalidates the callback rescheduled before it acquired the mutex");
        Expect(model.sends == sendsBefore && !model.pump.snapshot().executing,
               "executing callback sends nothing after stop");
    }

    void TestDisconnectAndReconnectStaleness()
    {
        Model model;
        model.start();
        Expect(model.open(), "disconnect model opens owner");
        const std::uintptr_t oldToken = model.pump.request(model.generation).token;
        model.preStop();
        Expect(model.pump.begin(oldToken) == BeginResult::Stale, "queued callback before disconnect is stale");
        Expect(model.lifecycle.stopSuccess(model.handle, model.generation), "disconnect model stops successfully");
        ++model.generation;
        Expect(model.lifecycle.beginRunning(model.handle, model.generation), "immediate reconnect commits new generation");
        Expect(model.pump.activate(model.generation), "reconnect activates pump generation");
        Expect(model.pump.begin(model.generation, oldToken) == BeginResult::Stale,
               "old callback cannot affect immediate reconnect");
    }

    void TestServerAndConnectionGenerationsAreIndependent()
    {
        State lifecycle;
        D2B_HTTPD_SEND_PUMP::State pump;
        Expect(lifecycle.beginRunning(0x1800, 33), "server lifecycle generation commits once");
        Expect(pump.activate(1), "first connection generation activates pump");
        Expect(lifecycle.accepting(0x1800, 33), "first connection is accepted by server gate");
        Expect(pump.invalidate(1), "first connection disconnect invalidates pump");
        Expect(pump.activate(2), "second connection gets a new pump generation");
        Expect(lifecycle.accepting(0x1800, 33),
               "same running server accepts a new connection generation");
        Expect(lifecycle.snapshot().generation == 33 && pump.snapshot().generation == 2,
               "server and connection generations remain distinct");
    }

    void TestFdReuseAba()
    {
        State lifecycle;
        D2B_HTTPD_SEND_PUMP::State pump;
        Expect(lifecycle.beginRunning(0x2000, 1), "ABA generation one runs");
        Expect(pump.activate(1), "ABA generation one pump activates");
        const std::uintptr_t oldToken = pump.request(1).token;
        Expect(lifecycle.beginStop(0x2000, 1) == BeginStopResult::First, "ABA generation one pre-stops");
        Expect(lifecycle.stopSuccess(0x2000, 1), "ABA generation one stops");
        Expect(lifecycle.beginRunning(0x2000, 2), "same fd key gets a new generation");
        Expect(pump.activate(2), "same fd key pump gets new generation");
        Expect(pump.begin(1, oldToken) == BeginResult::Stale, "fd reuse cannot clear old generation token");
        Expect(lifecycle.beginStop(0x2000, 1) == BeginStopResult::WrongHandle,
               "old generation handle is rejected after fd reuse");
    }

    void TestRepeatedServerLifecycle()
    {
        State lifecycle;
        Expect(lifecycle.beginRunning(0x3000, 1), "first server start");
        Expect(!lifecycle.beginRunning(0x3000, 1), "repeated start is rejected while running");
        Expect(lifecycle.beginStop(0x3000, 1) == BeginStopResult::First, "first stop enters pre-stopping");
        Expect(lifecycle.beginStop(0x3000, 1) == BeginStopResult::Repeated, "repeated pre-stop is deterministic");
        Expect(lifecycle.stopSuccess(0x3000, 1), "first stop succeeds");
        Expect(lifecycle.beginStop(0x3000, 1) == BeginStopResult::Inactive,
               "repeated post-stop has no active lifecycle");
    }

    void TestShutdownQueueReject()
    {
        State lifecycle;
        D2B_HTTPD_SEND_PUMP::State pump;
        Expect(lifecycle.beginRunning(0x4000, 1), "queue shutdown server starts");
        Expect(pump.activate(1), "queue shutdown pump starts");
        Expect(lifecycle.beginStop(0x4000, 1) == BeginStopResult::First, "queue shutdown enters pre-stopping");
        Expect(pump.invalidate(1), "queue shutdown invalidates pump");
        Expect(pump.request(1).result == ScheduleResult::Inactive,
               "queue_work is rejected after shutdown begins");
    }

    void TestCloseCallbackTiming()
    {
        Model before;
        before.start();
        Expect(before.open() && before.startStream(true), "close-before-stop owner is active");
        Expect(before.requestCloseCallback(), "close callback before pre-stop acquires mutex");
        Expect(!before.ownerOpen && !before.streamActive && !before.producerActive && !before.mutexHeld,
               "close callback before pre-stop clears owner and producer safely");
        before.preStop();
        before.postStop(true);
        Expect(before.completed(), "close callback before stop reaches complete server cleanup");

        Model during;
        during.start();
        Expect(during.open() && during.startStream(true), "close-during-stop owner is active");
        const std::uintptr_t token = during.queueWork().token;
        Expect(during.pump.begin(during.generation, token) == BeginResult::Began,
               "close-during-stop callback is executing");
        during.mutexHeld = true;
        Expect(!during.requestCloseCallback() && during.closeWaiting,
               "close callback during send waits for the shared mutex");
        ++during.sends;
        Expect(during.pump.finish(during.generation, token, false).result == FinishResult::Idle,
               "close-during-stop send callback finishes before close");
        during.mutexHeld = false;
        Expect(during.requestCloseCallback(), "close callback runs after executing send releases mutex");
        during.preStop();
        during.postStop(true);
        Expect(during.completed(), "close callback during stop leaves no stale work");

        Model after;
        after.start();
        Expect(after.open(), "close-after-stop owner starts");
        after.preStop();
        after.postStop(true);
        Expect(after.requestCloseCallback(), "close callback after stop is idempotent");
        Expect(after.completed(), "close callback after stop preserves complete snapshot");
    }

    void TestFaultPointMatrix()
    {
        Model model;
        model.start();
        Expect(model.open() && model.startStream(true), "fault matrix opens and publishes producer session");

        const D2B_HTTPD_SEND_PUMP::ScheduleDecision queued = model.queueWork();
        Expect(queued.result == ScheduleResult::Accepted, "queue success creates one pending callback");
        Expect(model.queueWork().result == ScheduleResult::Coalesced,
               "queue duplicate is coalesced while callback is pending");
        Expect(model.pump.reject(model.generation, queued.token) && model.invariant() && !model.mutexHeld,
               "queue rejection releases pending callback and preserves invariant");

        const D2B_HTTPD_SEND_PUMP::ScheduleDecision delayed = model.queueWork();
        Expect(delayed.result == ScheduleResult::Accepted, "delayed queue can be retried after rejection");
        Expect(model.queueWorkCalls == 2 && model.stoppedHandleQueueWorkCalls == 0,
               "external queue_work is called only for Accepted pump decisions");
        Expect(model.pump.begin(model.generation, delayed.token) == BeginResult::Began,
               "delayed callback eventually begins");
        ++model.sends;
        Expect(model.pump.finish(model.generation, delayed.token, false).result == FinishResult::Idle &&
                   model.invariant() && !model.mutexHeld,
               "successful send callback completes without rescheduling");

        model.queuedOutputFrames = 3;
        const unsigned sendsBefore = model.sends;
        Expect(model.sendFrame(true, true), "send success consumes one output frame");
        Expect(model.sends == sendsBefore + 1 && model.queuedOutputFrames == 2,
               "successful send updates only the bounded output queue");
        Expect(!model.sendFrame(true, false) && !model.sendFrame(false, true),
               "send failure and invalid socket are rejected without a post-stop write");
        Expect(model.sends == sendsBefore + 1 && !model.mutexHeld,
               "send fault points release the send mutex");
        Expect(model.invariant(), "send failure and invalid socket preserve producer invariant before stop");

        model.preStop();
        Expect(model.queuedOutputFrames == 0 && !model.producerActive && !model.streamActive,
               "producer abort clears queued output and stream state");
        const unsigned queueCallsBeforeStop = model.queueWorkCalls;
        Expect(model.queueWork().result == ScheduleResult::Inactive && model.queueWorkCalls == queueCallsBeforeStop &&
                   model.stoppedHandleQueueWorkCalls == 0,
               "stopped handle never calls queue_work");
        const unsigned sendAttemptsBeforeStop = model.sendAttempts;
        Expect(!model.sendFrame(true, true) && model.sendAttempts == sendAttemptsBeforeStop &&
                   model.stoppedHandleSendAttempts == 0,
               "pre-stop blocks stopped-handle send before the low-level send call");
        model.postStop(true);
        Expect(model.completed(), "successful server stop reaches a complete zero-work snapshot");
        Expect(model.queueWork().result == ScheduleResult::Inactive && !model.sendFrame(true, true) &&
                   model.queueWorkCalls == queueCallsBeforeStop && model.sendAttempts == sendAttemptsBeforeStop &&
                   model.stoppedHandleQueueWorkCalls == 0 && model.stoppedHandleSendAttempts == 0,
               "post-stop stale handle cannot queue work or issue a send");
        Expect(model.requestPreStop(), "close callback after stop is an idempotent no-op");
        model.postStop(true);
        Expect(model.completed(), "repeated post-stop cleanup remains complete");

        Model failed;
        failed.start();
        Expect(failed.open(), "stop-failure matrix opens owner");
        failed.preStop();
        failed.postStop(false);
        Expect(failed.lifecycle.snapshot().phase == Phase::StopFailed && !failed.completed() && failed.invariant() &&
                   !failed.mutexHeld,
               "failed httpd_stop retains retryable server lifecycle with invariant and mutex released");
        Expect(failed.requestPreStop(), "retry stop acquires the same serialized pre-stop path");
        failed.postStop(true);
        Expect(failed.completed(), "stop retry succeeds with no owner, session, or queued work left");

        Model publicationFailure;
        publicationFailure.start();
        Expect(publicationFailure.open(), "producer publication failure opens owner");
        Expect(!publicationFailure.startStream(false) && publicationFailure.invariant() &&
                   !publicationFailure.producerActive && !publicationFailure.streamActive &&
                   !publicationFailure.mutexHeld,
               "producer start publication failure aborts producer and releases mutex");
        publicationFailure.preStop();
        publicationFailure.postStop(true);
        Expect(publicationFailure.completed(), "publication failure cleanup reaches complete snapshot");
    }

    void TestProducerStartAbortInterleavings()
    {
        for (unsigned point = 0; point < 3; ++point)
        {
            Model model;
            model.start();
            Expect(model.open(), "producer interleaving opens owner");
            if (point == 0)
            {
                Expect(model.startStream(true), "producer interleaving starts stream");
                model.preStop();
            }
            else if (point == 1)
            {
                Expect(!model.startStream(false), "producer interleaving aborts publication failure");
                model.preStop();
            }
            else
            {
                Expect(model.startStream(true), "producer interleaving starts before waiting stop");
                model.mutexHeld = true;
                Expect(!model.requestPreStop() && model.stopWaiting,
                       "producer interleaving stop waits for start/send mutex");
                model.mutexHeld = false;
                Expect(model.requestPreStop(), "producer interleaving aborts after mutex handoff");
            }
            Expect(model.invariant() && !model.producerActive && !model.streamActive && !model.mutexHeld,
                   "each producer start/abort failure point preserves invariant and releases mutex");
            model.postStop(true);
            Expect(model.completed(), "each producer interleaving reaches complete cleanup");
        }
    }

    void TestStopFailureRetry()
    {
        State lifecycle;
        Expect(lifecycle.beginRunning(0x5000, 1), "failure model server starts");
        Expect(lifecycle.beginStop(0x5000, 1) == BeginStopResult::First, "failure model enters pre-stopping");
        Expect(lifecycle.stopFailure(0x5000, 1), "failed httpd_stop retains lifecycle key");
        Expect(!lifecycle.accepting(0x5000, 1), "failed stop disables activation");
        Expect(lifecycle.beginStop(0x5000, 1) == BeginStopResult::Retry, "failed stop permits deterministic retry");
        Expect(lifecycle.stopSuccess(0x5000, 1), "retry stop succeeds");
        Expect(lifecycle.snapshot().phase == Phase::Stopped && lifecycle.snapshot().handleKey == 0,
               "successful retry clears stored handle");
    }

    void TestRepeatedCleanupAndFaultInvariants()
    {
        Model model;
        model.start();
        Expect(model.open() && model.startStream(true), "fault model stream starts");
        Expect(model.invariant(), "fault model starts with complete invariant");
        model.preStop();
        model.preStop();
        model.postStop(true);
        model.postStop(true);
        Expect(model.completed(), "all fault and repeated cleanup points reach a complete snapshot");
    }
} // namespace

int main()
{
    TestReceivePreStopOwnership();
    TestStartTransactionStopWins();
    TestStartCloseSerialization();
    TestOrderlyStreamReuse();
    TestPendingPumpStop();
    TestExecutingPumpStop();
    TestDisconnectAndReconnectStaleness();
    TestServerAndConnectionGenerationsAreIndependent();
    TestFdReuseAba();
    TestRepeatedServerLifecycle();
    TestShutdownQueueReject();
    TestCloseCallbackTiming();
    TestFaultPointMatrix();
    TestProducerStartAbortInterleavings();
    TestStopFailureRetry();
    TestRepeatedCleanupAndFaultInvariants();
    std::cout << "PASS: deterministic HTTPD stop lifecycle and send-pump serialization\n";
    return 0;
}
