#pragma once

#include <cstddef>
#include <cstdint>

namespace D2B
{
    static const std::size_t kEnvelopeSize = 32;
    static const std::size_t kViRecordSize = 16;
    static const std::size_t kSingleViFrameSize = kEnvelopeSize + kViRecordSize;

    enum FrameFlag
    {
        StreamStart = 0x01,
        StreamEnd = 0x02,
        Discontinuity = 0x04,
        ProducerOverflow = 0x08,
        OutputQueueDrop = 0x10,
        SourcePaused = 0x20,
        TimebaseReset = 0x40,
    };

    struct ViSample
    {
        std::uint64_t sequence;
        std::uint64_t timestampUs;
        std::uint32_t validMask;
        float voltageV;
        float currentA;
    };

    enum class FrameWriteError
    {
        None,
        InvalidArgument,
        BufferTooSmall,
        InvalidStreamId,
        InvalidFlags,
        InvalidSample,
        ArithmeticOverflow,
    };

    struct FrameWriteResult
    {
        FrameWriteError error;
        std::size_t size;

        bool ok() const { return error == FrameWriteError::None; }
    };

    bool CheckedAddU64(std::uint64_t left, std::uint64_t right, std::uint64_t& result);
    bool RequiredViFrameSize(std::uint32_t sampleCount, std::size_t& result);
    FrameWriteResult WriteSingleViFrame(std::uint8_t* output,
                                        std::size_t capacity,
                                        std::uint32_t streamId,
                                        std::uint8_t flags,
                                        const ViSample& sample);
    FrameWriteResult WriteStreamEndFrame(std::uint8_t* output,
                                         std::size_t capacity,
                                         std::uint32_t streamId,
                                         std::uint64_t nextSequence,
                                         std::uint64_t timestampUs);
} // namespace D2B
