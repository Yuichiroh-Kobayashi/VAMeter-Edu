#include "d2b_frame_writer.h"
#include "d2b_output_ring.h"

#include <cstdint>
#include <cstdio>

namespace
{
    bool PrintPrepared(std::uint64_t sequence,
                       std::uint64_t timestampUs,
                       std::uint8_t writerFlags,
                       std::uint8_t pendingFlags,
                       bool first,
                       std::uint64_t expected)
    {
        D2B::OutputFrame frame = {};
        const D2B::ViSample sample = {sequence, timestampUs, 3, 3.25F, -0.125F};
        const D2B::FrameWriteResult write =
            D2B::WriteSingleViFrame(frame.data, sizeof(frame.data), 11, writerFlags, sample);
        if (!write.ok())
            return false;
        frame.size = write.size;
        frame.sequence = sequence;
        frame.timestampUs = timestampUs;

        std::uint8_t prepared[D2B::kSingleViFrameSize];
        std::uint64_t nextSequence = 0;
        if (!D2B::PrepareOutputFrame(frame,
                                     pendingFlags,
                                     first,
                                     expected,
                                     prepared,
                                     sizeof(prepared),
                                     nextSequence))
            return false;
        for (std::size_t index = 0; index < sizeof(prepared); ++index)
            std::printf("%02x", prepared[index]);
        std::printf("\n");
        return nextSequence == sequence + 1;
    }
} // namespace

int main()
{
    if (!PrintPrepared(100, 1000000, 0, 0, true, 0))
        return 1;
    if (!PrintPrepared(105,
                       1005000,
                       D2B::Discontinuity | D2B::ProducerOverflow,
                       D2B::Discontinuity | D2B::OutputQueueDrop,
                       false,
                       101))
        return 1;
    return 0;
}
