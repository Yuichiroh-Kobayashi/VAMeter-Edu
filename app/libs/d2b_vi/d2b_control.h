#pragma once

#include <cstddef>
#include <cstdint>

namespace D2B
{
    static const std::size_t kMaximumControlMessageSize = 2048;
    static const std::size_t kMaximumBinaryFrameSize = 48;

    enum class ErrorCode
    {
        None,
        Busy,
        Unauthorized,
        UnknownStream,
        UnsupportedVersion,
        UnsupportedProfile,
        UnsupportedParameters,
        InvalidMessage,
        InvalidState,
        FrameTooLarge,
        InternalError,
    };

    const char* ErrorCodeName(ErrorCode code);

    enum class ControlState
    {
        Connected,
        Ready,
        Streaming,
        Closed,
    };

    enum class ClientMessageType
    {
        Hello,
        StartStream,
        StopStream,
        Ping,
    };

    enum class Profile
    {
        ViMeasurement,
        PcmAudio,
    };

    struct ClientMessage
    {
        ClientMessageType type;
        Profile profile;
        bool supportsVersion01;
        bool hasStreamId;
        std::uint32_t streamId;
        std::uint16_t correlationBytes;
        char stream[129];
        char correlation[513];
    };

    struct ParseResult
    {
        ErrorCode error;
        ClientMessage message;

        bool ok() const { return error == ErrorCode::None; }
    };

    ErrorCode ParseClientMessageInto(const std::uint8_t* data, std::size_t size, ParseResult& output);

    // Compatibility wrapper for host callers. The product control hot path uses
    // ParseClientMessageInto() so the caller owns the large result storage.
    ParseResult ParseClientMessage(const std::uint8_t* data, std::size_t size);
    ErrorCode ValidateClientMessageState(const ClientMessage& message, ControlState state, bool ownsStream);
} // namespace D2B
