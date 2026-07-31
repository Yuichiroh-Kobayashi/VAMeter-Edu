#include "d2b_frame_writer.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>

namespace
{
    void PrintHex(const std::uint8_t* data, std::size_t size)
    {
        for (std::size_t index = 0; index < size; ++index)
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(data[index]);
        std::cout << '\n';
    }
} // namespace

int main()
{
    std::uint8_t output[D2B::kSingleViFrameSize];
    const D2B::ViSample first = {100, 1000000, 3, 3.25F, -0.125F};
    D2B::FrameWriteResult result = D2B::WriteSingleViFrame(output, sizeof(output), 11, D2B::StreamStart, first);
    if (!result.ok())
        return 1;
    PrintHex(output, result.size);

    result = D2B::WriteStreamEndFrame(output, sizeof(output), 11, 102, 1003000);
    if (!result.ok())
        return 1;
    PrintHex(output, result.size);

    const D2B::ViSample invalidCurrent = {0, 500, 1, 3.25F, std::numeric_limits<float>::quiet_NaN()};
    result = D2B::WriteSingleViFrame(output,
                                     sizeof(output),
                                     11,
                                     D2B::StreamStart | D2B::Discontinuity | D2B::TimebaseReset,
                                     invalidCurrent);
    if (!result.ok())
        return 1;
    PrintHex(output, result.size);
    return 0;
}
