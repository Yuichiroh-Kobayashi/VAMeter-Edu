#include "d2b_frame_writer.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace D2B
{
    namespace
    {
        static const std::uint8_t kTimestampedSamplesFrameType = 0x02;
        static const std::uint8_t kStreamEndFrameType = 0x10;
        static const std::uint8_t kCauseFlags = ProducerOverflow | OutputQueueDrop | SourcePaused | TimebaseReset;

        void StoreLe32(std::uint8_t* output, std::uint32_t value)
        {
            output[0] = static_cast<std::uint8_t>(value);
            output[1] = static_cast<std::uint8_t>(value >> 8);
            output[2] = static_cast<std::uint8_t>(value >> 16);
            output[3] = static_cast<std::uint8_t>(value >> 24);
        }

        void StoreLe64(std::uint8_t* output, std::uint64_t value)
        {
            StoreLe32(output, static_cast<std::uint32_t>(value));
            StoreLe32(output + 4, static_cast<std::uint32_t>(value >> 32));
        }

        void StoreFloatLe(std::uint8_t* output, float value)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            StoreLe32(output, bits);
        }

        void WriteEnvelope(std::uint8_t* output,
                           std::uint8_t frameType,
                           std::uint8_t flags,
                           std::uint32_t streamId,
                           std::uint32_t sampleCount,
                           std::uint64_t firstSequence,
                           std::uint64_t firstTimestampUs)
        {
            output[0] = 'D';
            output[1] = '2';
            output[2] = 'B';
            output[3] = 'S';
            output[4] = 0;
            output[5] = 1;
            output[6] = frameType;
            output[7] = flags;
            StoreLe32(output + 8, streamId);
            StoreLe32(output + 12, sampleCount);
            StoreLe64(output + 16, firstSequence);
            StoreLe64(output + 24, firstTimestampUs);
        }

        bool ValidDataFlags(std::uint8_t flags)
        {
            if ((flags & 0x80U) != 0 || (flags & StreamEnd) != 0)
                return false;
            if ((flags & kCauseFlags) != 0 && (flags & Discontinuity) == 0)
                return false;
            if ((flags & TimebaseReset) != 0 && (flags & StreamStart) == 0)
                return false;
            return true;
        }

        FrameWriteResult Failure(FrameWriteError error)
        {
            FrameWriteResult result = {error, 0};
            return result;
        }
    } // namespace

    static_assert(sizeof(float) == sizeof(std::uint32_t), "V/I frame writer requires 32-bit float");
    static_assert(std::numeric_limits<float>::is_iec559, "V/I frame writer requires IEEE 754 float");

    bool CheckedAddU64(std::uint64_t left, std::uint64_t right, std::uint64_t& result)
    {
        if (left > std::numeric_limits<std::uint64_t>::max() - right)
            return false;
        result = left + right;
        return true;
    }

    bool RequiredViFrameSize(std::uint32_t sampleCount, std::size_t& result)
    {
        const std::size_t maximumProtocolFrameSize =
            std::numeric_limits<std::size_t>::max() < std::numeric_limits<std::uint32_t>::max()
                ? std::numeric_limits<std::size_t>::max()
                : std::numeric_limits<std::uint32_t>::max();
        if (sampleCount == 0 || sampleCount > (maximumProtocolFrameSize - kEnvelopeSize) / kViRecordSize)
            return false;
        result = kEnvelopeSize + static_cast<std::size_t>(sampleCount) * kViRecordSize;
        return true;
    }

    FrameWriteResult WriteSingleViFrame(std::uint8_t* output,
                                        std::size_t capacity,
                                        std::uint32_t streamId,
                                        std::uint8_t flags,
                                        const ViSample& sample)
    {
        if (output == 0)
            return Failure(FrameWriteError::InvalidArgument);
        std::size_t requiredSize = 0;
        if (!RequiredViFrameSize(1, requiredSize))
            return Failure(FrameWriteError::ArithmeticOverflow);
        if (capacity < requiredSize)
            return Failure(FrameWriteError::BufferTooSmall);
        if (streamId == 0)
            return Failure(FrameWriteError::InvalidStreamId);
        if (!ValidDataFlags(flags))
            return Failure(FrameWriteError::InvalidFlags);
        if ((sample.validMask & ~3U) != 0 ||
            ((sample.validMask & 1U) != 0 && !std::isfinite(sample.voltageV)) ||
            ((sample.validMask & 2U) != 0 && !std::isfinite(sample.currentA)))
            return Failure(FrameWriteError::InvalidSample);

        std::uint64_t lastSequence = 0;
        if (!CheckedAddU64(sample.sequence, 0, lastSequence))
            return Failure(FrameWriteError::ArithmeticOverflow);
        (void)lastSequence;

        std::uint8_t frame[kSingleViFrameSize];
        WriteEnvelope(frame, kTimestampedSamplesFrameType, flags, streamId, 1, sample.sequence, sample.timestampUs);
        StoreLe32(frame + kEnvelopeSize, 0);
        StoreLe32(frame + kEnvelopeSize + 4, sample.validMask);
        StoreFloatLe(frame + kEnvelopeSize + 8, (sample.validMask & 1U) != 0 ? sample.voltageV : 0.0F);
        StoreFloatLe(frame + kEnvelopeSize + 12, (sample.validMask & 2U) != 0 ? sample.currentA : 0.0F);
        std::memcpy(output, frame, sizeof(frame));
        FrameWriteResult result = {FrameWriteError::None, sizeof(frame)};
        return result;
    }

    FrameWriteResult WriteStreamEndFrame(std::uint8_t* output,
                                         std::size_t capacity,
                                         std::uint32_t streamId,
                                         std::uint64_t nextSequence,
                                         std::uint64_t timestampUs)
    {
        if (output == 0)
            return Failure(FrameWriteError::InvalidArgument);
        if (capacity < kEnvelopeSize)
            return Failure(FrameWriteError::BufferTooSmall);
        if (streamId == 0)
            return Failure(FrameWriteError::InvalidStreamId);

        std::uint8_t frame[kEnvelopeSize];
        WriteEnvelope(frame, kStreamEndFrameType, StreamEnd, streamId, 0, nextSequence, timestampUs);
        std::memcpy(output, frame, sizeof(frame));
        FrameWriteResult result = {FrameWriteError::None, sizeof(frame)};
        return result;
    }
} // namespace D2B
