#include "d2b_frame_writer.h"
#include "d2b_httpd_send_pump_state.h"
#include "d2b_acquisition.h"
#include "d2b_output_ring.h"

#include <cstdlib>
#include <cstring>
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

    D2B::OutputFrame MakeFrame(std::uint64_t sequence, std::uint8_t flags = 0)
    {
        D2B::OutputFrame frame = {};
        const D2B::ViSample sample = {sequence, 1000 + sequence, D2B::kVoltageValid | D2B::kCurrentValid, 1.0F, 2.0F};
        const D2B::FrameWriteResult result =
            D2B::WriteSingleViFrame(frame.data, sizeof(frame.data), 7, flags, sample);
        Expect(result.ok(), "test frame is encoded");
        frame.size = result.size;
        frame.sequence = sequence;
        frame.timestampUs = sample.timestampUs;
        return frame;
    }

    std::uint64_t Sequence(const std::uint8_t* frame)
    {
        std::uint64_t value = 0;
        for (unsigned index = 0; index != 8; ++index)
            value |= static_cast<std::uint64_t>(frame[16 + index]) << (index * 8);
        return value;
    }

    void TestBoundedLifecycle()
    {
        using namespace D2B_HTTPD_SEND_PUMP;
        State state;
        Expect(state.activate(11), "first generation activates");

        const ScheduleDecision first = state.request(11);
        Expect(first.result == ScheduleResult::Accepted && first.token != 0, "first request accepted with token");
        const ScheduleDecision duplicate = state.request(11);
        Expect(duplicate.result == ScheduleResult::Coalesced && duplicate.token == 0,
               "duplicate request coalesces without a second token");
        Expect(state.snapshot().pending && state.snapshot().pendingToken == first.token,
               "exactly one pending owner remains");

        Expect(state.begin(first.token) == BeginResult::Began, "current callback begins");
        const FinishDecision reschedule = state.finish(first.token, true);
        Expect(reschedule.result == FinishResult::Rescheduled && reschedule.token != 0 &&
                   reschedule.token != first.token,
               "callback transfers ownership to one new token");
        Expect(state.snapshot().pending && !state.snapshot().executing &&
                   state.snapshot().pendingToken == reschedule.token,
               "reschedule leaves one pending owner");

        Expect(state.begin(reschedule.token) == BeginResult::Began, "rescheduled callback begins");
        Expect(state.finish(reschedule.token, false).result == FinishResult::Idle,
               "callback becomes idle when work is drained");
        Expect(!state.snapshot().pending && !state.snapshot().executing, "idle has no ownership");

        const ScheduleDecision failureRequest = state.request(11);
        Expect(failureRequest.result == ScheduleResult::Accepted, "request after idle accepted");
        Expect(state.begin(failureRequest.token) == BeginResult::Began, "failing callback begins");
        Expect(state.invalidate(11), "callback failure can invalidate its generation");
        Expect(state.begin(failureRequest.token) == BeginResult::Stale, "invalidated callback is stale");
        Expect(state.activate(12), "new generation recovers after callback failure");
        Expect(state.request(12).result == ScheduleResult::Accepted, "later request is accepted");

        State rejection;
        Expect(rejection.activate(21), "rejection generation activates");
        ScheduleDecision rejected = rejection.request(21);
        Expect(rejection.reject(rejected.token), "initial queue rejection clears matching token");
        Expect(rejection.request(21).result == ScheduleResult::Accepted, "initial rejection remains retryable");
        rejected = rejection.request(21);
        Expect(rejected.result == ScheduleResult::Coalesced, "duplicate retry coalesces");
        const std::uintptr_t pending = rejection.snapshot().pendingToken;
        Expect(rejection.begin(pending) == BeginResult::Began, "reschedule rejection callback begins");
        const FinishDecision next = rejection.finish(pending, true);
        Expect(next.result == FinishResult::Rescheduled, "reschedule creates one token");
        Expect(rejection.reject(next.token), "reschedule queue rejection clears matching token");
        Expect(rejection.request(21).result == ScheduleResult::Accepted, "reschedule rejection remains retryable");
    }

    void TestGenerationStaleness()
    {
        using namespace D2B_HTTPD_SEND_PUMP;
        State state;
        Expect(state.activate(31), "disconnect test generation activates");
        const std::uintptr_t oldToken = state.request(31).token;
        Expect(state.invalidate(31), "disconnect invalidates matching generation");
        Expect(state.begin(oldToken) == BeginResult::Stale, "disconnect makes old callback stale");
        Expect(state.request(31).result == ScheduleResult::Inactive, "old generation cannot reschedule");

        Expect(state.activate(32), "server restart activates new generation");
        const std::uintptr_t newToken = state.request(32).token;
        Expect(newToken != 0, "new generation gets a token");
        Expect(state.begin(31, oldToken) == BeginResult::Stale,
               "old generation callback cannot clear newer pending token");
        Expect(state.finish(31, oldToken, false).result == FinishResult::Stale,
               "old generation finish is stale");
        Expect(!state.reject(31, oldToken), "old generation rejection is stale");
        Expect(state.begin(oldToken) == BeginResult::Stale, "old callback cannot clear newer pending token");
        Expect(state.finish(oldToken, false).result == FinishResult::Stale, "old token finish is stale");
        Expect(!state.reject(oldToken), "old token rejection is stale");
        Expect(state.snapshot().pending && state.snapshot().pendingToken == newToken &&
                   !state.snapshot().executing,
               "stale operations leave newer pending token intact");
        Expect(state.begin(32, newToken) == BeginResult::Began, "new callback begins");
        Expect(state.finish(32, newToken, false).result == FinishResult::Idle, "new callback can finish");

        const ScheduleDecision later = state.request(32);
        Expect(later.result == ScheduleResult::Accepted, "later scheduling remains possible");
        Expect(state.begin(32, later.token) == BeginResult::Began, "later callback begins");
        Expect(state.finish(32, later.token, false).result == FinishResult::Idle,
               "later callback completes");

        const std::uintptr_t stopToken = state.request(32).token;
        Expect(state.invalidate(32), "stop invalidates active owner");
        Expect(state.activate(33), "reconnect after stop activates");
        Expect(state.request(33).result == ScheduleResult::Accepted, "stop cannot permanently block reconnect");
        Expect(state.begin(stopToken) == BeginResult::Stale, "stopped callback cannot affect reconnect");
    }

    void TestOrderlyStreamReuse()
    {
        using namespace D2B_HTTPD_SEND_PUMP;
        State state;
        Expect(state.activate(51), "orderly stream connection activates once");

        const ScheduleDecision first = state.request(51);
        Expect(first.result == ScheduleResult::Accepted, "first stream schedules a callback");
        Expect(state.begin(51, first.token) == BeginResult::Began, "first stream callback begins");
        Expect(state.finishOrderlyStream(51, first.token).result == FinishResult::Idle,
               "orderly stream completion finishes current callback without invalidating connection pump");
        Snapshot idle = state.snapshot();
        Expect(idle.active && idle.generation == 51 && !idle.pending && !idle.executing,
               "orderly stream completion leaves active idle pump for same connection");

        const ScheduleDecision second = state.request(51);
        Expect(second.result == ScheduleResult::Accepted, "second stream on same connection is accepted");
        Expect(state.begin(51, second.token) == BeginResult::Began, "second stream callback begins");
        Expect(state.finishOrderlyStream(51, second.token).result == FinishResult::Idle,
               "second orderly stream completion returns pump to idle");
        idle = state.snapshot();
        Expect(idle.active && !idle.pending && !idle.executing,
               "connection pump remains reusable after two orderly streams");

        const ScheduleDecision closePending = state.request(51);
        Expect(closePending.result == ScheduleResult::Accepted, "close regression schedules stale callback");
        Expect(state.invalidate(51), "connection close invalidates pump ownership");
        Expect(state.request(51).result == ScheduleResult::Inactive,
               "closed connection rejects queue_work after orderly reuse");
        Expect(state.begin(51, closePending.token) == BeginResult::Stale,
               "closed connection makes pending callback stale");
    }

    void TestBoundedFifoAndMetadata()
    {
        using namespace D2B_HTTPD_SEND_PUMP;
        State state;
        state.activate(41);
        const std::uintptr_t token = state.request(41).token;
        Expect(state.begin(token) == BeginResult::Began, "bounded pump begins");

        D2B::OutputRing ring;
        for (std::uint64_t sequence = 0; sequence != D2B::kOutputQueueDepth; ++sequence)
        {
            const std::uint8_t flags = sequence == 0 ? D2B::Discontinuity | D2B::ProducerOverflow : 0;
            Expect(!ring.pushDropOldest(MakeFrame(sequence, flags)), "ring accepts initial FIFO frames");
        }
        Expect(ring.pushDropOldest(MakeFrame(32)) && ring.outputQueueDropCount() == 1,
               "full output ring drops oldest and counts it");

        std::uint64_t expectedSequence = 0;
        bool first = true;
        for (unsigned count = 0; count != 4; ++count)
        {
            D2B::OutputFrame frame = {};
            std::uint8_t pendingFlags = 0;
            Expect(ring.popForTransport(frame, pendingFlags), "bounded callback pops a frame");
            Expect(frame.sequence == static_cast<std::uint64_t>(count + 1), "bounded callback preserves FIFO order");
            std::uint8_t prepared[D2B::kSingleViFrameSize] = {};
            std::uint64_t nextSequence = 0;
            Expect(D2B::PrepareOutputFrame(frame,
                                           pendingFlags,
                                           first,
                                           expectedSequence,
                                           prepared,
                                           sizeof(prepared),
                                           nextSequence),
                   "bounded callback prepares frame");
            if (first)
            {
                Expect((prepared[7] & D2B::StreamStart) != 0, "first frame retains stream start");
                Expect((prepared[7] & D2B::Discontinuity) != 0 &&
                           (prepared[7] & D2B::ProducerOverflow) != 0 &&
                           (prepared[7] & D2B::OutputQueueDrop) != 0,
                       "producer overflow and output-drop metadata are carried");
                first = false;
            }
            Expect(Sequence(prepared) == frame.sequence, "prepared sequence remains exact");
            expectedSequence = nextSequence;
        }

        const FinishDecision next = state.finish(token, true);
        Expect(next.result == FinishResult::Rescheduled && next.token != 0, "four-frame bound reschedules once");
        Expect(state.begin(next.token) == BeginResult::Began, "follow-up callback begins");
        unsigned remaining = 0;
        while (ring.queuedCount() != 0)
        {
            D2B::OutputFrame frame = {};
            std::uint8_t pendingFlags = 0;
            Expect(ring.popForTransport(frame, pendingFlags), "follow-up drains retained frame");
            Expect(frame.sequence == static_cast<std::uint64_t>(remaining + 5), "follow-up FIFO sequence remains exact");
            ++remaining;
        }
        Expect(remaining == 28, "drop-oldest leaves exactly twenty-eight after first batch");
        Expect(state.finish(next.token, false).result == FinishResult::Idle, "final callback becomes idle");
    }
} // namespace

int main()
{
    TestBoundedLifecycle();
    TestGenerationStaleness();
    TestOrderlyStreamReuse();
    TestBoundedFifoAndMetadata();
    std::cout << "PASS: bounded HTTPD send-pump lifecycle and D2B FIFO metadata\n";
    return 0;
}
