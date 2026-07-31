#include "d2b_frame_writer.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

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

    std::uint32_t LoadLe32(const std::uint8_t* input)
    {
        return static_cast<std::uint32_t>(input[0]) | (static_cast<std::uint32_t>(input[1]) << 8) |
               (static_cast<std::uint32_t>(input[2]) << 16) | (static_cast<std::uint32_t>(input[3]) << 24);
    }
} // namespace

int main()
{
    static const std::uint8_t expectedFirst[D2B::kSingleViFrameSize] = {
        0x44, 0x32, 0x42, 0x53, 0x00, 0x01, 0x02, 0x01, 0x0b, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x42, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x40, 0x00, 0x00, 0x00, 0xbe,
    };
    const D2B::ViSample first = {100, 1000000, 3, 3.25F, -0.125F};
    std::uint8_t output[D2B::kSingleViFrameSize];
    std::memset(output, 0xa5, sizeof(output));
    D2B::FrameWriteResult result = D2B::WriteSingleViFrame(output, sizeof(output), 11, D2B::StreamStart, first);
    Expect(result.ok() && result.size == sizeof(output), "first frame is written");
    Expect(std::memcmp(output, expectedFirst, sizeof(output)) == 0, "first frame matches oracle golden bytes");

    std::uint8_t unchanged[D2B::kSingleViFrameSize];
    std::memset(unchanged, 0xa5, sizeof(unchanged));
    const D2B::ViSample badMask = {100, 1000000, 4, 1.0F, 1.0F};
    result = D2B::WriteSingleViFrame(unchanged, sizeof(unchanged), 11, D2B::StreamStart, badMask);
    Expect(result.error == D2B::FrameWriteError::InvalidSample && unchanged[0] == 0xa5,
           "invalid mask leaves output unchanged");

    const D2B::ViSample validNan = {100, 1000000, 1, std::numeric_limits<float>::quiet_NaN(), 0.0F};
    result = D2B::WriteSingleViFrame(unchanged, sizeof(unchanged), 11, D2B::StreamStart, validNan);
    Expect(result.error == D2B::FrameWriteError::InvalidSample && unchanged[0] == 0xa5,
           "valid non-finite channel is rejected without output mutation");

    const D2B::ViSample invalidNan = {101,
                                      1000100,
                                      1,
                                      2.0F,
                                      std::numeric_limits<float>::quiet_NaN()};
    result = D2B::WriteSingleViFrame(output, sizeof(output), 11, 0, invalidNan);
    Expect(result.ok() && LoadLe32(output + 44) == 0, "invalid channel is canonical positive zero");

    result = D2B::WriteSingleViFrame(output, sizeof(output), 11, D2B::ProducerOverflow, first);
    Expect(result.error == D2B::FrameWriteError::InvalidFlags, "cause flag requires discontinuity");
    result = D2B::WriteSingleViFrame(output,
                                     sizeof(output),
                                     11,
                                     D2B::Discontinuity | D2B::TimebaseReset,
                                     first);
    Expect(result.error == D2B::FrameWriteError::InvalidFlags, "timebase reset requires stream start");
    result = D2B::WriteSingleViFrame(output,
                                     sizeof(output),
                                     11,
                                     D2B::StreamStart | D2B::Discontinuity | D2B::TimebaseReset,
                                     first);
    Expect(result.ok(), "new-session timebase reset flags are accepted");

    std::memset(unchanged, 0xa5, sizeof(unchanged));
    result = D2B::WriteSingleViFrame(unchanged, D2B::kSingleViFrameSize - 1, 11, D2B::StreamStart, first);
    Expect(result.error == D2B::FrameWriteError::BufferTooSmall && unchanged[0] == 0xa5,
           "short output buffer remains unchanged");

    static const std::uint8_t expectedEnd[D2B::kEnvelopeSize] = {
        0x44, 0x32, 0x42, 0x53, 0x00, 0x01, 0x10, 0x02, 0x0b, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xf8, 0x4d, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    result = D2B::WriteStreamEndFrame(output, sizeof(output), 11, 102, 1003000);
    Expect(result.ok() && result.size == D2B::kEnvelopeSize &&
               std::memcmp(output, expectedEnd, D2B::kEnvelopeSize) == 0,
           "STREAM_END matches oracle golden bytes");

    std::uint64_t sum = 0;
    Expect(D2B::CheckedAddU64(std::numeric_limits<std::uint64_t>::max() - 1, 1, sum) &&
               sum == std::numeric_limits<std::uint64_t>::max(),
           "checked uint64 addition accepts exact maximum");
    Expect(!D2B::CheckedAddU64(std::numeric_limits<std::uint64_t>::max(), 1, sum),
           "checked uint64 addition rejects overflow");
    std::size_t frameSize = 0;
    Expect(!D2B::RequiredViFrameSize(0, frameSize) && D2B::RequiredViFrameSize(1, frameSize) &&
               frameSize == D2B::kSingleViFrameSize,
           "V/I frame size equation is checked");
    Expect(!D2B::RequiredViFrameSize(std::numeric_limits<std::uint32_t>::max(), frameSize),
           "V/I frame size rejects protocol uint32 overflow");

    std::cout << "PASS: d2b V/I explicit little-endian frame writer\n";
    return 0;
}
