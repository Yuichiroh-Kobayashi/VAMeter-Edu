#pragma once

#include "d2b_control.h"

#include <cstddef>
#include <cstdint>

namespace D2B
{
    enum class ClientFrameType
    {
        Text,
        Continuation,
        Binary,
    };

    enum class ReassemblyResult
    {
        NeedMore,
        Complete,
        BinaryRejected,
        InvalidSequence,
        TooLarge,
    };

    class MessageBuffer
    {
    public:
        MessageBuffer();

        ReassemblyResult accept(ClientFrameType type, bool final, const std::uint8_t* payload, std::size_t size);
        void reset();

        const std::uint8_t* data() const { return _data; }
        std::size_t size() const { return _size; }
        std::size_t remaining() const { return kMaximumControlMessageSize - _size; }
        bool assembling() const { return _assembling; }

    private:
        std::uint8_t _data[kMaximumControlMessageSize];
        std::size_t _size;
        bool _assembling;
    };
} // namespace D2B
