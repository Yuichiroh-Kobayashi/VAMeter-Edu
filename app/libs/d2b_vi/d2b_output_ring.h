#pragma once

#include "d2b_frame_writer.h"

#include <cstddef>
#include <cstdint>

namespace D2B
{
    static const std::size_t kOutputQueueDepth = 32;

    struct OutputFrame
    {
        std::uint8_t data[kSingleViFrameSize];
        std::size_t size;
        std::uint64_t sequence;
        std::uint64_t timestampUs;
    };

    class OutputRing
    {
    public:
        OutputRing();

        bool pushDropOldest(const OutputFrame& frame);
        bool popForTransport(OutputFrame& frame, std::uint8_t& pendingFlags);
        void clear();

        std::size_t queuedCount() const { return _count; }
        std::uint64_t outputQueueDropCount() const { return _outputQueueDropCount; }

    private:
        OutputFrame _frames[kOutputQueueDepth];
        std::size_t _head;
        std::size_t _count;
        std::uint64_t _outputQueueDropCount;
        std::uint8_t _pendingFlags;
    };

    bool PrepareOutputFrame(const OutputFrame& frame,
                            std::uint8_t pendingFlags,
                            bool firstTransportFrame,
                            std::uint64_t expectedSequence,
                            std::uint8_t* output,
                            std::size_t capacity,
                            std::uint64_t& nextSequence);
} // namespace D2B
