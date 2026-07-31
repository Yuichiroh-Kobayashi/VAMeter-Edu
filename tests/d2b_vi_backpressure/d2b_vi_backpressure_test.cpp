#include "d2b_frame_writer.h"
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
        const D2B::ViSample sample = {sequence, 1000 + sequence, 3, 3.25F, -0.125F};
        const D2B::FrameWriteResult result =
            D2B::WriteSingleViFrame(frame.data, sizeof(frame.data), 11, flags, sample);
        Expect(result.ok(), "test frame encodes");
        frame.size = result.size;
        frame.sequence = sample.sequence;
        frame.timestampUs = sample.timestampUs;
        return frame;
    }
} // namespace

int main()
{
    D2B::OutputRing ring;
    for (std::uint64_t sequence = 0; sequence < D2B::kOutputQueueDepth; ++sequence)
    {
        const std::uint8_t flags = sequence == 0 ? D2B::Discontinuity | D2B::ProducerOverflow : 0;
        Expect(!ring.pushDropOldest(MakeFrame(sequence, flags)), "output ring accepts frames before capacity");
    }
    Expect(ring.queuedCount() == D2B::kOutputQueueDepth && ring.outputQueueDropCount() == 0,
           "output ring reaches exact depth without loss");
    Expect(ring.pushDropOldest(MakeFrame(32)) && ring.queuedCount() == D2B::kOutputQueueDepth &&
               ring.outputQueueDropCount() == 1,
           "full output ring drops exactly one oldest frame");

    D2B::OutputFrame retained = {};
    std::uint8_t pendingFlags = 0;
    Expect(ring.popForTransport(retained, pendingFlags) && retained.sequence == 1,
           "transport accepts the first retained sequence");
    Expect(pendingFlags == (D2B::Discontinuity | D2B::ProducerOverflow | D2B::OutputQueueDrop),
           "dropped frame cause and output-queue cause carry forward");

    std::uint8_t prepared[D2B::kSingleViFrameSize] = {};
    std::uint64_t nextSequence = 0;
    Expect(D2B::PrepareOutputFrame(retained,
                                   pendingFlags,
                                   true,
                                   0,
                                   prepared,
                                   sizeof(prepared),
                                   nextSequence) &&
               prepared[7] == (D2B::StreamStart | D2B::Discontinuity | D2B::ProducerOverflow |
                               D2B::OutputQueueDrop) &&
               nextSequence == 2,
           "first transported frame receives STREAM_START and all latched causes");

    Expect(ring.popForTransport(retained, pendingFlags) && retained.sequence == 2 && pendingFlags == 0,
           "latched output causes are consumed once at transport acceptance");
    Expect(D2B::PrepareOutputFrame(retained, 0, false, 2, prepared, sizeof(prepared), nextSequence) &&
               prepared[7] == 0 && nextSequence == 3,
           "continuous frame has no discontinuity");

    const D2B::OutputFrame gap = MakeFrame(5);
    Expect(D2B::PrepareOutputFrame(gap, 0, false, 3, prepared, sizeof(prepared), nextSequence) &&
               prepared[7] == D2B::Discontinuity && nextSequence == 6,
           "unexplained forward sequence gap is explicit");
    std::memset(prepared, 0xa5, sizeof(prepared));
    Expect(!D2B::PrepareOutputFrame(MakeFrame(2), 0, false, 3, prepared, sizeof(prepared), nextSequence) &&
               prepared[0] == 0xa5,
           "sequence regression is rejected without partial output mutation");
    D2B::OutputFrame inconsistent = MakeFrame(9);
    inconsistent.data[16] = 8;
    Expect(!D2B::PrepareOutputFrame(inconsistent, 0, false, 9, prepared, sizeof(prepared), nextSequence) &&
               prepared[0] == 0xa5,
           "inconsistent encoded metadata is rejected without partial mutation");

    ring.clear();
    Expect(ring.queuedCount() == 0 && ring.outputQueueDropCount() == 1 &&
               !ring.popForTransport(retained, pendingFlags),
           "session clear preserves the uptime-cumulative output drop counter");

    std::cout << "PASS: d2b V/I output backpressure and transport flag latching\n";
    return 0;
}
