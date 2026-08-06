#include "d2b_control.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
    int HexDigit(char value)
    {
        if (value >= '0' && value <= '9')
            return value - '0';
        if (value >= 'a' && value <= 'f')
            return value - 'a' + 10;
        if (value >= 'A' && value <= 'F')
            return value - 'A' + 10;
        return -1;
    }

    bool DecodeHex(const char* text, std::vector<std::uint8_t>& data)
    {
        const std::size_t length = std::strlen(text);
        if ((length & 1U) != 0)
            return false;
        data.resize(length / 2);
        for (std::size_t index = 0; index < data.size(); ++index)
        {
            const int high = HexDigit(text[index * 2]);
            const int low = HexDigit(text[index * 2 + 1]);
            if (high < 0 || low < 0)
                return false;
            data[index] = static_cast<std::uint8_t>((high << 4) | low);
        }
        return true;
    }

    bool ParseState(const char* text, D2B::ControlState& state)
    {
        if (std::strcmp(text, "CONNECTED") == 0)
            state = D2B::ControlState::Connected;
        else if (std::strcmp(text, "READY") == 0)
            state = D2B::ControlState::Ready;
        else if (std::strcmp(text, "STREAMING") == 0)
            state = D2B::ControlState::Streaming;
        else if (std::strcmp(text, "CLOSED") == 0)
            state = D2B::ControlState::Closed;
        else
            return false;
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 4)
        return 2;

    std::vector<std::uint8_t> data;
    if (!DecodeHex(argv[1], data))
        return 2;

    D2B::ParseResult parsed = {};
    D2B::ParseClientMessageInto(data.data(), data.size(), parsed);
    const D2B::ParseResult wrapped = D2B::ParseClientMessage(data.data(), data.size());
    if (wrapped.error != parsed.error || std::memcmp(&wrapped.message, &parsed.message, sizeof(parsed.message)) != 0)
        return 3;
    D2B::ErrorCode result = parsed.error;
    if (result == D2B::ErrorCode::None && argc == 4)
    {
        D2B::ControlState state;
        if (!ParseState(argv[2], state) || (std::strcmp(argv[3], "0") != 0 && std::strcmp(argv[3], "1") != 0))
            return 2;
        result = D2B::ValidateClientMessageState(parsed.message, state, std::strcmp(argv[3], "1") == 0);
    }

    std::cout << (result == D2B::ErrorCode::None ? "none" : D2B::ErrorCodeName(result)) << '\n';
    return 0;
}
