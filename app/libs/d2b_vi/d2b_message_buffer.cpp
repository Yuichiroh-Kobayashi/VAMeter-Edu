#include "d2b_message_buffer.h"

#include <cstring>

namespace D2B
{
    MessageBuffer::MessageBuffer() : _size(0), _assembling(false) {}

    ReassemblyResult MessageBuffer::accept(ClientFrameType type,
                                             bool final,
                                             const std::uint8_t* payload,
                                             std::size_t size)
    {
        if (type == ClientFrameType::Binary)
        {
            reset();
            return ReassemblyResult::BinaryRejected;
        }

        if (type == ClientFrameType::Text)
        {
            if (_assembling)
            {
                reset();
                return ReassemblyResult::InvalidSequence;
            }
            _size = 0;
        }
        else if (!_assembling)
        {
            reset();
            return ReassemblyResult::InvalidSequence;
        }

        if (size > remaining())
        {
            reset();
            return ReassemblyResult::TooLarge;
        }
        if (size != 0)
        {
            if (payload == 0)
            {
                reset();
                return ReassemblyResult::InvalidSequence;
            }
            std::memcpy(_data + _size, payload, size);
            _size += size;
        }

        _assembling = !final;
        return final ? ReassemblyResult::Complete : ReassemblyResult::NeedMore;
    }

    void MessageBuffer::reset()
    {
        _size = 0;
        _assembling = false;
    }
} // namespace D2B
