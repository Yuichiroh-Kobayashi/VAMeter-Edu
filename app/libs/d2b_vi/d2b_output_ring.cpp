#include "d2b_output_ring.h"

#include <cstring>
#include <limits>

namespace D2B
{
    namespace
    {
        static const std::uint8_t kCauseFlags = ProducerOverflow | OutputQueueDrop | SourcePaused | TimebaseReset;
        static const std::uint8_t kCarryFlags = Discontinuity | kCauseFlags;

        std::uint32_t LoadLe32(const std::uint8_t* input)
        {
            return static_cast<std::uint32_t>(input[0]) |
                   static_cast<std::uint32_t>(input[1]) << 8 |
                   static_cast<std::uint32_t>(input[2]) << 16 |
                   static_cast<std::uint32_t>(input[3]) << 24;
        }

        std::uint64_t LoadLe64(const std::uint8_t* input)
        {
            return static_cast<std::uint64_t>(LoadLe32(input)) |
                   static_cast<std::uint64_t>(LoadLe32(input + 4)) << 32;
        }
    } // namespace

    OutputRing::OutputRing() : _head(0), _count(0), _outputQueueDropCount(0), _pendingFlags(0) {}

    bool OutputRing::pushDropOldest(const OutputFrame& frame)
    {
        bool dropped = false;
        if (_count == kOutputQueueDepth)
        {
            _pendingFlags |= _frames[_head].data[7] & kCarryFlags;
            _pendingFlags |= Discontinuity | OutputQueueDrop;
            _head = (_head + 1) % kOutputQueueDepth;
            --_count;
            if (_outputQueueDropCount != std::numeric_limits<std::uint64_t>::max())
                ++_outputQueueDropCount;
            dropped = true;
        }
        const std::size_t writeIndex = (_head + _count) % kOutputQueueDepth;
        _frames[writeIndex] = frame;
        ++_count;
        return dropped;
    }

    bool OutputRing::popForTransport(OutputFrame& frame, std::uint8_t& pendingFlags)
    {
        if (_count == 0)
            return false;
        frame = _frames[_head];
        _head = (_head + 1) % kOutputQueueDepth;
        --_count;
        pendingFlags = _pendingFlags;
        _pendingFlags = 0;
        return true;
    }

    void OutputRing::clear()
    {
        _head = 0;
        _count = 0;
        _pendingFlags = 0;
    }

    bool PrepareOutputFrame(const OutputFrame& frame,
                            std::uint8_t pendingFlags,
                            bool firstTransportFrame,
                            std::uint64_t expectedSequence,
                            std::uint8_t* output,
                            std::size_t capacity,
                            std::uint64_t& nextSequence)
    {
        if (output == nullptr || capacity < kSingleViFrameSize || frame.size != kSingleViFrameSize ||
            frame.data[0] != 'D' || frame.data[1] != '2' || frame.data[2] != 'B' || frame.data[3] != 'S' ||
            frame.data[4] != 0 || frame.data[5] != 1 || frame.data[6] != 0x02 ||
            (frame.data[7] & (StreamStart | StreamEnd | 0x80U)) != 0 || LoadLe32(frame.data + 8) == 0 ||
            LoadLe32(frame.data + 12) != 1 || LoadLe64(frame.data + 16) != frame.sequence ||
            LoadLe64(frame.data + 24) != frame.timestampUs ||
            (pendingFlags & ~(Discontinuity | kCauseFlags)) != 0 ||
            (!firstTransportFrame && frame.sequence < expectedSequence) ||
            frame.sequence == std::numeric_limits<std::uint64_t>::max())
            return false;

        std::uint8_t prepared[kSingleViFrameSize];
        std::memcpy(prepared, frame.data, sizeof(prepared));
        std::uint8_t flags = prepared[7] | pendingFlags;
        if (firstTransportFrame)
            flags |= StreamStart;
        else if (frame.sequence > expectedSequence)
            flags |= Discontinuity;
        if ((flags & kCauseFlags) != 0)
            flags |= Discontinuity;
        if ((flags & TimebaseReset) != 0 && (flags & StreamStart) == 0)
            return false;

        prepared[7] = flags;
        nextSequence = frame.sequence + 1;
        std::memcpy(output, prepared, sizeof(prepared));
        return true;
    }
} // namespace D2B
