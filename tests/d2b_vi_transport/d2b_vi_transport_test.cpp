#include "d2b_control.h"
#include "d2b_message_buffer.h"
#include "d2b_session.h"
#include "d2b_stream_start_disposition.h"
#include "d2b_transport_runtime_state.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

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

    D2B::ParseResult Parse(const std::string& message)
    {
        const std::uint8_t* data = reinterpret_cast<const std::uint8_t*>(message.data());
        const D2B::ParseResult wrapped = D2B::ParseClientMessage(data, message.size());
        D2B::ParseResult into;
        std::memset(&into, 0xa5, sizeof(into));
        const D2B::ErrorCode error = D2B::ParseClientMessageInto(data, message.size(), into);
        Expect(error == into.error && wrapped.error == into.error,
               "ParseClientMessage wrapper and Into error codes match");
        Expect(std::memcmp(&wrapped.message, &into.message, sizeof(into.message)) == 0,
               "ParseClientMessage wrapper and Into messages match bytewise");
        return wrapped;
    }

    void ExpectError(const std::string& message, D2B::ErrorCode error, const char* detail)
    {
        Expect(Parse(message).error == error, detail);
    }

    D2B::ControlResponse Handle(D2B::Session& session,
                                const D2B::ClientMessage& message,
                                std::uint32_t& streamIdCounter)
    {
        D2B::Session intoSession = session;
        std::uint32_t intoCounter = streamIdCounter;
        const D2B::ControlResponse wrapped = D2B::HandleClientMessage(session, message, streamIdCounter);
        D2B::ControlResponse into;
        std::memset(&into, 0xa5, sizeof(into));
        D2B::HandleClientMessageInto(intoSession, message, intoCounter, into);
        Expect(wrapped.error == into.error && wrapped.size == into.size,
               "HandleClientMessage wrapper and Into metadata match");
        Expect(std::memcmp(wrapped.data, into.data, sizeof(into.data)) == 0,
               "HandleClientMessage wrapper and Into response bytes match");
        Expect(session.state == intoSession.state && session.ownsStream == intoSession.ownsStream &&
                   session.streamId == intoSession.streamId && streamIdCounter == intoCounter,
               "HandleClientMessage wrapper and Into state transitions match");
        return wrapped;
    }

    void VerifyErrorResponseEquivalence()
    {
        const D2B::ErrorCode errors[] = {
            D2B::ErrorCode::None,
            D2B::ErrorCode::Busy,
            D2B::ErrorCode::Unauthorized,
            D2B::ErrorCode::UnknownStream,
            D2B::ErrorCode::UnsupportedVersion,
            D2B::ErrorCode::UnsupportedProfile,
            D2B::ErrorCode::UnsupportedParameters,
            D2B::ErrorCode::InvalidMessage,
            D2B::ErrorCode::InvalidState,
            D2B::ErrorCode::FrameTooLarge,
            D2B::ErrorCode::InternalError,
        };
        const char* const expected[] = {
            "{\"type\":\"error\",\"code\":\"\",\"message\":\"\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"busy\",\"message\":\"stream already owned\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"unauthorized\",\"message\":\"authentication failed\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"unknown_stream\",\"message\":\"stream is not supported\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"unsupported_version\",\"message\":\"version is not supported\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"unsupported_profile\",\"message\":\"profile is not supported\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"unsupported_parameters\",\"message\":\"parameters are not supported\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"invalid_message\",\"message\":\"control message is invalid\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"invalid_state\",\"message\":\"message is invalid in the current state\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"frame_too_large\",\"message\":\"control message is too large\",\"recoverable\":true}",
            "{\"type\":\"error\",\"code\":\"internal_error\",\"message\":\"internal error\",\"recoverable\":true}",
        };
        for (std::size_t index = 0; index < sizeof(errors) / sizeof(errors[0]); ++index)
        {
            const D2B::ControlResponse wrapped = D2B::BuildErrorResponse(errors[index]);
            D2B::ControlResponse into;
            std::memset(&into, 0xa5, sizeof(into));
            D2B::BuildErrorResponseInto(errors[index], into);
            Expect(wrapped.error == into.error && wrapped.size == into.size,
                   "BuildErrorResponse wrapper and Into metadata match");
            Expect(std::memcmp(wrapped.data, into.data, sizeof(into.data)) == 0,
                   "BuildErrorResponse wrapper and Into bytes match");
            Expect(std::string(into.data, into.size) == expected[index],
                   "BuildErrorResponse preserves the protocol byte sequence");
        }
    }

    D2B::ErrorCode ParseInto(const std::string& text, D2B::ParseResult& workspace)
    {
        return D2B::ParseClientMessageInto(reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), workspace);
    }

    void ExpectResponseTailClear(const D2B::ControlResponse& response, const char* detail)
    {
        bool clear = response.size < sizeof(response.data) && response.data[response.size] == '\0';
        for (std::size_t index = response.size + 1; clear && index < sizeof(response.data); ++index)
            clear = response.data[index] == '\0';
        Expect(clear, detail);
    }

    void VerifyWorkspaceReuseAndBoundaries()
    {
        static const char hello[] =
            "{\"type\":\"hello\",\"protocol\":\"d2b-stream\",\"versions\":[\"0.1\"]}";
        static const char start[] =
            "{\"type\":\"start_stream\",\"stream\":\"live-vi\",\"profile\":\"vi-measurement\","
            "\"parameters\":{\"sample_format\":\"vi-f32le\",\"channel_count\":2,\"channel_mask\":3,"
            "\"sample_rate\":{\"numerator\":0,\"denominator\":0}}}";
        static const char stop[] = "{\"type\":\"stop_stream\"}";
        static const char shortPing[] = "{\"type\":\"ping\",\"correlation\":\"x\"}";

        D2B::ParseResult parsed;
        std::memset(&parsed, 0xa5, sizeof(parsed));
        std::string escapedCorrelation;
        for (std::size_t index = 0; index < 128; ++index)
            escapedCorrelation += "\\u0001";
        const std::string maximumPing =
            "{\"type\":\"ping\",\"correlation\":\"" + escapedCorrelation + "\"}";
        Expect(ParseInto(maximumPing, parsed) == D2B::ErrorCode::None && parsed.message.correlationBytes == 128,
               "maximum correlation and worst-case JSON escaping parse into reused workspace");
        for (std::size_t index = 0; index < parsed.message.correlationBytes; ++index)
            Expect(parsed.message.correlation[index] == '\x01', "maximum escaped correlation bytes are retained");

        const D2B::ClientMessage zeroMessage = {};
        Expect(ParseInto("{\"type\":\"ping\",\"correlation\":\"x\",\"unknown\":1}", parsed) ==
                   D2B::ErrorCode::InvalidMessage &&
                   std::memcmp(&parsed.message, &zeroMessage, sizeof(zeroMessage)) == 0,
               "invalid message clears prior maximum correlation");
        Expect(ParseInto(hello, parsed) == D2B::ErrorCode::None && parsed.message.type == D2B::ClientMessageType::Hello &&
                   parsed.message.correlation[0] == '\0' && parsed.message.stream[0] == '\0',
               "hello has no stale correlation or stream");
        Expect(ParseInto(start, parsed) == D2B::ErrorCode::None &&
                   std::strcmp(parsed.message.stream, "live-vi") == 0 && !parsed.message.hasStreamId,
               "start has no stale stream id");
        Expect(ParseInto(stop, parsed) == D2B::ErrorCode::None && !parsed.message.hasStreamId &&
                   parsed.message.streamId == 0 && parsed.message.stream[0] == '\0',
               "stop has no stale stream name or id");
        Expect(ParseInto(shortPing, parsed) == D2B::ErrorCode::None && parsed.message.correlationBytes == 1 &&
                   parsed.message.correlation[0] == 'x' && parsed.message.correlation[1] == '\0' &&
                   parsed.message.correlation[2] == '\0',
               "short ping clears prior maximum correlation tail");

        const std::string minimalPing = shortPing;
        std::string exact2048 = minimalPing;
        exact2048.append(D2B::kMaximumControlMessageSize - exact2048.size(), ' ');
        Expect(exact2048.size() == D2B::kMaximumControlMessageSize &&
                   ParseInto(exact2048, parsed) == D2B::ErrorCode::None,
               "2048-byte control message boundary is accepted");
        exact2048.push_back(' ');
        Expect(ParseInto(exact2048, parsed) == D2B::ErrorCode::FrameTooLarge,
               "2049-byte control message is rejected");

        std::string stream128(128, 'a');
        const std::string startPrefix =
            "{\"type\":\"start_stream\",\"stream\":\"";
        const std::string startSuffix =
            "\",\"profile\":\"vi-measurement\",\"parameters\":{\"sample_format\":\"vi-f32le\","
            "\"channel_count\":2,\"channel_mask\":3,\"sample_rate\":{\"numerator\":0,\"denominator\":0}}}";
        Expect(ParseInto(startPrefix + stream128 + startSuffix, parsed) == D2B::ErrorCode::None &&
                   std::strlen(parsed.message.stream) == 128,
               "128-character stream name boundary is accepted");
        stream128.push_back('a');
        Expect(ParseInto(startPrefix + stream128 + startSuffix, parsed) == D2B::ErrorCode::InvalidMessage,
               "129-character stream name is rejected");

        std::string token256(256, 't');
        const std::string authPrefix =
            "{\"type\":\"hello\",\"protocol\":\"d2b-stream\",\"versions\":[\"0.1\"],"
            "\"authentication\":{\"scheme\":\"pairing-token\",\"token\":\"";
        const std::string authSuffix = "\"}}";
        Expect(ParseInto(authPrefix + token256 + authSuffix, parsed) == D2B::ErrorCode::None,
               "256-character authentication token boundary is accepted");
        token256.push_back('t');
        Expect(ParseInto(authPrefix + token256 + authSuffix, parsed) == D2B::ErrorCode::InvalidMessage,
               "257-character authentication token is rejected");

        D2B::Session session = {};
        std::uint32_t streamIdCounter = 0;
        D2B::ControlResponse response;
        D2B::OpenSession(session);
        Expect(ParseInto("{\"type\":\"hello\",\"protocol\":\"d2b-stream\",\"versions\":[\"1.0\"]}", parsed) ==
                   D2B::ErrorCode::None,
               "unsupported-version hello is structurally valid");
        D2B::HandleClientMessageInto(session, parsed.message, streamIdCounter, response);
        Expect(response.error == D2B::ErrorCode::UnsupportedVersion && session.state == D2B::ControlState::Connected,
               "unsupported version does not contaminate session");
        Expect(ParseInto(hello, parsed) == D2B::ErrorCode::None, "fresh hello parses after unsupported version");
        D2B::HandleClientMessageInto(session, parsed.message, streamIdCounter, response);
        Expect(response.ok() && session.state == D2B::ControlState::Ready,
               "fresh hello succeeds after unsupported version");
        ExpectResponseTailClear(response, "welcome response clears reused response tail");

        for (std::size_t operation = 0; operation < 2500; ++operation)
        {
            Expect(ParseInto(shortPing, parsed) == D2B::ErrorCode::None, "ready ping parses during 10,000-op reuse");
            D2B::HandleClientMessageInto(session, parsed.message, streamIdCounter, response);
            Expect(response.ok() && session.state == D2B::ControlState::Ready,
                   "ready ping remains ready during 10,000-op reuse");
            ExpectResponseTailClear(response, "pong response clears reused response tail");

            Expect(ParseInto(start, parsed) == D2B::ErrorCode::None, "start parses during 10,000-op reuse");
            D2B::HandleClientMessageInto(session, parsed.message, streamIdCounter, response);
            Expect(response.ok() && session.state == D2B::ControlState::Streaming &&
                       session.streamId == operation + 1,
                   "start allocates fresh stream id during 10,000-op reuse");

            Expect(ParseInto(shortPing, parsed) == D2B::ErrorCode::None,
                   "streaming ping parses during 10,000-op reuse");
            D2B::HandleClientMessageInto(session, parsed.message, streamIdCounter, response);
            Expect(response.ok() && session.state == D2B::ControlState::Streaming,
                   "streaming ping preserves stream during 10,000-op reuse");

            Expect(ParseInto(stop, parsed) == D2B::ErrorCode::None, "stop parses during 10,000-op reuse");
            D2B::HandleClientMessageInto(session, parsed.message, streamIdCounter, response);
            Expect(response.ok() && session.state == D2B::ControlState::Ready && session.streamId == 0,
                   "stop clears stream during 10,000-op reuse");
        }
        Expect(streamIdCounter == 2500, "10,000 reused control operations retain exact stream-id sequence");
    }

    void VerifyThreeProtocolViolations()
    {
        D2B::ControlViolationState violations;
        Expect(violations.count() == 0, "violation tracker starts clear");
        Expect(!violations.recordFailure() && violations.count() == 1,
               "first protocol violation keeps the connection open");
        Expect(!violations.recordFailure() && violations.count() == 2,
               "second protocol violation keeps the connection open");
        Expect(violations.recordFailure() && violations.count() == 3,
               "third protocol violation closes the connection");
        Expect(violations.recordFailure() && violations.count() == 3,
               "violation count saturates after the close threshold");
        violations.reset();
        Expect(violations.count() == 0, "successful control processing clears protocol violations");
    }
} // namespace

int main()
{
    using D2B::ClientMessageType;
    using D2B::ControlState;
    using D2B::ErrorCode;
    using D2B::Profile;

    VerifyErrorResponseEquivalence();
    VerifyWorkspaceReuseAndBoundaries();
    VerifyThreeProtocolViolations();

    Expect(D2B::DecideStreamStartDisposition(true) == D2B::StreamStartDisposition::ContinueConnection,
           "successful publication keeps the connection");
    Expect(D2B::DecideStreamStartDisposition(false) == D2B::StreamStartDisposition::CloseConnection,
           "publication failure closes the handler connection");

    const D2B::PublicStreamingSnapshot coherent = {
        true,
        true,
        7,
        static_cast<std::uintptr_t>(0x1234),
        9,
        true,
        true,
        true,
        false,
        false,
        7,
        true,
        7,
        true,
        static_cast<std::uintptr_t>(0x1234),
        9,
    };
    Expect(D2B::IsPublicStreaming(coherent), "status is streaming only for coherent owner snapshots");
    D2B::PublicStreamingSnapshot stopFailed = coherent;
    stopFailed.lifecycleAccepting = false;
    Expect(!D2B::IsPublicStreaming(stopFailed), "stop-failed lifecycle reports idle");
    D2B::PublicStreamingSnapshot mismatched = coherent;
    mismatched.producerStreamId = 8;
    Expect(!D2B::IsPublicStreaming(mismatched), "stream ID mismatch reports idle");

    const D2B::ParseResult hello = Parse("{\"type\":\"hello\",\"protocol\":\"d2b-stream\",\"versions\":[\"1.0\",\"0.1\"]}");
    Expect(hello.ok() && hello.message.type == ClientMessageType::Hello && hello.message.supportsVersion01,
           "valid hello selects 0.1");
    Expect(D2B::ValidateClientMessageState(hello.message, ControlState::Connected, false) == ErrorCode::None,
           "hello is accepted while connected");
    Expect(D2B::ValidateClientMessageState(hello.message, ControlState::Ready, false) == ErrorCode::InvalidState,
           "second hello is invalid state");

    const std::string viStart =
        "{\"type\":\"start_stream\",\"stream\":\"live-vi\",\"profile\":\"vi-measurement\","
        "\"parameters\":{\"sample_format\":\"vi-f32le\",\"channel_count\":2,\"channel_mask\":3,"
        "\"sample_rate\":{\"numerator\":0,\"denominator\":0}}}";
    const D2B::ParseResult vi = Parse(viStart);
    Expect(vi.ok() && vi.message.type == ClientMessageType::StartStream && vi.message.profile == Profile::ViMeasurement &&
               std::strcmp(vi.message.stream, "live-vi") == 0,
           "valid V/I start is decoded");
    Expect(D2B::ValidateClientMessageState(vi.message, ControlState::Connected, false) == ErrorCode::InvalidState,
           "start before hello is invalid state");
    Expect(D2B::ValidateClientMessageState(vi.message, ControlState::Ready, false) == ErrorCode::None,
           "start is accepted while ready");

    const std::string pcmStart =
        "{\"type\":\"start_stream\",\"stream\":\"audio-0\",\"profile\":\"pcm-audio\","
        "\"parameters\":{\"sample_format\":\"pcm-s16le-interleaved\",\"channel_count\":1,\"channel_mask\":1,"
        "\"sample_rate\":{\"numerator\":16000,\"denominator\":1},\"samples_per_frame\":256}}";
    Expect(Parse(pcmStart).ok(), "standard PCM shape is parser-valid before device profile filtering");

    ExpectError("{\"type\":\"ping\",\"correlation\":\"a\",\"correlation\":\"b\"}",
                ErrorCode::InvalidMessage,
                "duplicate key is rejected");
    ExpectError("{\"type\":\"ping\",\"correlation\":\"a\",\"\\u0063orrelation\":\"b\"}",
                ErrorCode::InvalidMessage,
                "escaped duplicate key is rejected");
    ExpectError("{\"type\":\"ping\",\"correlation\":NaN}", ErrorCode::InvalidMessage, "NaN is rejected");
    ExpectError("{\"type\":\"ping\",\"correlation\":Infinity}", ErrorCode::InvalidMessage, "Infinity is rejected");
    ExpectError("{\"type\":\"ping\",\"correlation\":\"a\"} false", ErrorCode::InvalidMessage, "trailing JSON is rejected");
    ExpectError("{\"type\":\"stop_stream\",\"stream_id\":32.0}", ErrorCode::InvalidMessage, "float integer is rejected");
    ExpectError("{\"type\":\"stop_stream\",\"stream_id\":4294967296}", ErrorCode::InvalidMessage, "uint32 overflow is rejected");
    ExpectError("{\"type\":\"hello\",\"protocol\":\"d2b-stream\",\"versions\":[\"0.1\",\"0.1\"]}",
                ErrorCode::InvalidMessage,
                "duplicate version is rejected");
    ExpectError("{\"type\":\"start_stream\",\"stream\":\"live-vi\",\"profile\":\"example\",\"parameters\":{}}",
                ErrorCode::UnsupportedProfile,
                "unknown profile is rejected semantically");

    std::string oversized(D2B::kMaximumControlMessageSize + 1, ' ');
    ExpectError(oversized, ErrorCode::FrameTooLarge, "raw byte limit is enforced before JSON");
    const std::uint8_t invalidUtf8[] = {0xff};
    D2B::ParseResult invalidUtf8Result;
    std::memset(&invalidUtf8Result, 0xa5, sizeof(invalidUtf8Result));
    Expect(D2B::ParseClientMessageInto(invalidUtf8, sizeof(invalidUtf8), invalidUtf8Result) ==
               ErrorCode::InvalidMessage &&
               invalidUtf8Result.error == ErrorCode::InvalidMessage,
           "invalid UTF-8 is rejected");

    const D2B::ParseResult ping = Parse("{\"type\":\"ping\",\"correlation\":\"lesson-1\"}");
    Expect(ping.ok() && ping.message.correlationBytes == 8 && std::strcmp(ping.message.correlation, "lesson-1") == 0,
           "ping correlation is retained");
    Expect(D2B::ValidateClientMessageState(ping.message, ControlState::Ready, false) == ErrorCode::None,
           "ping is valid while ready");
    Expect(D2B::ValidateClientMessageState(ping.message, ControlState::Closed, false) == ErrorCode::InvalidState,
           "closed state rejects messages");

    const D2B::ParseResult stop = Parse("{\"type\":\"stop_stream\",\"stream_id\":7,\"reason\":\"done\"}");
    Expect(stop.ok() && stop.message.hasStreamId && stop.message.streamId == 7, "stop fields are retained");
    Expect(D2B::ValidateClientMessageState(stop.message, ControlState::Streaming, false) == ErrorCode::InvalidState,
           "non-owner stop is rejected");
    Expect(D2B::ValidateClientMessageState(stop.message, ControlState::Streaming, true) == ErrorCode::None,
           "owner stop is accepted");

    D2B::Session session = {};
    std::uint32_t streamIdCounter = 0;
    D2B::OpenSession(session);
    D2B::ControlResponse response = Handle(session, hello.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Ready &&
               std::string(response.data, response.size) ==
                   "{\"type\":\"welcome\",\"protocol\":\"d2b-stream\",\"version\":\"0.1\","
                   "\"max_control_message_size\":2048,\"max_binary_frame_size\":48,"
                   "\"session_state\":\"ready\",\"server_name\":\"VAMeter-Edu\"}",
           "hello produces welcome and enters ready");

    const D2B::ParseResult unsupportedHello =
        Parse("{\"type\":\"hello\",\"protocol\":\"d2b-stream\",\"versions\":[\"1.0\"]}");
    D2B::Session secondSession = {};
    D2B::OpenSession(secondSession);
    response = Handle(secondSession, unsupportedHello.message, streamIdCounter);
    Expect(response.error == ErrorCode::UnsupportedVersion && secondSession.state == ControlState::Connected,
           "unsupported version leaves state unchanged");

    response = Handle(session, vi.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Streaming && session.ownsStream && session.streamId == 1 &&
               std::string(response.data, response.size) ==
                   "{\"type\":\"stream_started\",\"stream\":\"live-vi\",\"profile\":\"vi-measurement\","
                   "\"parameters\":{\"sample_format\":\"vi-f32le\",\"channel_count\":2,\"channel_mask\":3,"
                   "\"sample_rate\":{\"numerator\":0,\"denominator\":0}},\"stream_id\":1}",
           "V/I start allocates a nonzero stream ID");
    const std::uint32_t activeStreamId = session.streamId;
    response = Handle(session, ping.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Streaming && session.streamId == activeStreamId,
           "ping does not change streaming state");

    const D2B::ParseResult wrongStop = Parse("{\"type\":\"stop_stream\",\"stream_id\":2}");
    response = Handle(session, wrongStop.message, streamIdCounter);
    Expect(response.error == ErrorCode::UnknownStream && session.state == ControlState::Streaming &&
               session.streamId == activeStreamId,
           "wrong stream ID leaves ownership unchanged");
    response = Handle(session, stop.message, streamIdCounter);
    Expect(response.error == ErrorCode::UnknownStream && session.state == ControlState::Streaming,
           "another wrong stream ID leaves state unchanged");
    const D2B::ParseResult matchingStop = Parse("{\"type\":\"stop_stream\",\"stream_id\":1}");
    response = Handle(session, matchingStop.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Ready && !session.ownsStream && session.streamId == 0 &&
               std::string(response.data, response.size) ==
                   "{\"type\":\"stream_stopped\",\"stream_id\":1,\"reason\":\"client request\"}",
           "matching stop releases ownership");
    response = Handle(session, vi.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Streaming && session.ownsStream && session.streamId == 2,
           "orderly stop permits a second stream on the same connection with a new id");
    const D2B::ParseResult reconnectStop = Parse("{\"type\":\"stop_stream\",\"stream_id\":2}");
    response = Handle(session, reconnectStop.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Ready && !session.ownsStream && session.streamId == 0,
           "second orderly stream returns the connection to ready");

    D2B::Session publicationFailed = {};
    D2B::OpenSession(publicationFailed);
    response = Handle(publicationFailed, hello.message, streamIdCounter);
    Expect(response.ok() && publicationFailed.state == ControlState::Ready,
           "publication-failure session completes hello before stream start");
    response = Handle(publicationFailed, vi.message, streamIdCounter);
    Expect(response.ok() && publicationFailed.state == ControlState::Streaming && publicationFailed.streamId == 3,
           "production session allocates an id before publication callback");
    Expect(D2B::DecideStreamStartDisposition(false) == D2B::StreamStartDisposition::CloseConnection,
           "failed publication selects handler close disposition");
    D2B::CloseSession(publicationFailed);

    D2B::Session reconnected = {};
    D2B::OpenSession(reconnected);
    response = Handle(reconnected, hello.message, streamIdCounter);
    Expect(response.ok() && reconnected.state == ControlState::Ready,
           "reconnect opens a new production session");
    response = Handle(reconnected, vi.message, streamIdCounter);
    Expect(response.ok() && reconnected.state == ControlState::Streaming && reconnected.streamId == 4,
           "reconnect receives a fresh persistent stream id after publication close");
    D2B::CloseSession(reconnected);

    const D2B::ParseResult nulPing = Parse("{\"type\":\"ping\",\"correlation\":\"a\\u0000b\"}");
    response = Handle(session, nulPing.message, streamIdCounter);
    Expect(response.ok() && std::string(response.data, response.size) == "{\"type\":\"pong\",\"correlation\":\"a\\u0000b\"}",
           "embedded NUL correlation is retained and escaped");
    D2B::CloseSession(session);
    Expect(session.state == ControlState::Closed && !session.ownsStream && session.streamId == 0,
           "disconnect clears stream ownership");

    D2B::MessageBuffer fragments;
    const std::uint8_t first[] = {'{', '\"', 't'};
    const std::uint8_t second[] = {'y', 'p', 'e', '\"', ':', '\"', 'p', 'i', 'n', 'g', '\"', ',', '\"',
                                   'c', 'o', 'r', 'r', 'e', 'l', 'a', 't', 'i', 'o', 'n', '\"', ':', '\"', 'x',
                                   '\"', '}'};
    Expect(fragments.accept(D2B::ClientFrameType::Text, false, first, sizeof(first)) ==
               D2B::ReassemblyResult::NeedMore &&
               fragments.assembling(),
           "first text fragment begins bounded reassembly");
    Expect(fragments.accept(D2B::ClientFrameType::Continuation, true, second, sizeof(second)) ==
               D2B::ReassemblyResult::Complete &&
               Parse(std::string(reinterpret_cast<const char*>(fragments.data()), fragments.size())).ok(),
           "continuation completes one valid control message");

    fragments.reset();
    std::string maximum(D2B::kMaximumControlMessageSize, 'x');
    Expect(fragments.accept(D2B::ClientFrameType::Text,
                            false,
                            reinterpret_cast<const std::uint8_t*>(maximum.data()),
                            maximum.size()) == D2B::ReassemblyResult::NeedMore,
           "exact raw-message limit can be buffered");
    const std::uint8_t extra = 'x';
    Expect(fragments.accept(D2B::ClientFrameType::Continuation, true, &extra, 1) == D2B::ReassemblyResult::TooLarge &&
               fragments.size() == 0 && !fragments.assembling(),
           "fragmented overflow is rejected before copying extra payload");
    Expect(fragments.accept(D2B::ClientFrameType::Continuation, true, &extra, 1) ==
               D2B::ReassemblyResult::InvalidSequence,
           "orphan continuation is rejected");
    Expect(fragments.accept(D2B::ClientFrameType::Binary, true, &extra, 1) ==
               D2B::ReassemblyResult::BinaryRejected,
           "client binary frame is rejected");

    std::cout << "PASS: d2b V/I strict control parser, session, and bounded reassembly\n";
    return 0;
}
