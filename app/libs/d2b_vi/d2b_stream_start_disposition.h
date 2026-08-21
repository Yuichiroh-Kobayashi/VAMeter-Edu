#pragma once

#include <cstdint>

namespace D2B
{
    enum class StreamStartDisposition : std::uint8_t
    {
        ContinueConnection,
        CloseConnection,
    };

    StreamStartDisposition DecideStreamStartDisposition(bool pipelineStartSucceeded);
} // namespace D2B
