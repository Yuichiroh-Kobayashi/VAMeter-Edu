#include "d2b_control.h"
#include "d2b_message_buffer.h"
#include "d2b_session.h"

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
        return D2B::ParseClientMessage(reinterpret_cast<const std::uint8_t*>(message.data()), message.size());
    }

    void ExpectError(const std::string& message, D2B::ErrorCode error, const char* detail)
    {
        Expect(Parse(message).error == error, detail);
    }
} // namespace

int main()
{
    using D2B::ClientMessageType;
    using D2B::ControlState;
    using D2B::ErrorCode;
    using D2B::Profile;

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
    Expect(D2B::ParseClientMessage(invalidUtf8, sizeof(invalidUtf8)).error == ErrorCode::InvalidMessage,
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
    D2B::ControlResponse response = D2B::HandleClientMessage(session, hello.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Ready &&
               std::string(response.data, response.size).find("\"type\":\"welcome\"") != std::string::npos,
           "hello produces welcome and enters ready");

    const D2B::ParseResult unsupportedHello =
        Parse("{\"type\":\"hello\",\"protocol\":\"d2b-stream\",\"versions\":[\"1.0\"]}");
    D2B::Session secondSession = {};
    D2B::OpenSession(secondSession);
    response = D2B::HandleClientMessage(secondSession, unsupportedHello.message, streamIdCounter);
    Expect(response.error == ErrorCode::UnsupportedVersion && secondSession.state == ControlState::Connected,
           "unsupported version leaves state unchanged");

    response = D2B::HandleClientMessage(session, vi.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Streaming && session.ownsStream && session.streamId == 1 &&
               std::string(response.data, response.size).find("\"stream_id\":1") != std::string::npos,
           "V/I start allocates a nonzero stream ID");
    const std::uint32_t activeStreamId = session.streamId;
    response = D2B::HandleClientMessage(session, ping.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Streaming && session.streamId == activeStreamId,
           "ping does not change streaming state");

    const D2B::ParseResult wrongStop = Parse("{\"type\":\"stop_stream\",\"stream_id\":2}");
    response = D2B::HandleClientMessage(session, wrongStop.message, streamIdCounter);
    Expect(response.error == ErrorCode::UnknownStream && session.state == ControlState::Streaming &&
               session.streamId == activeStreamId,
           "wrong stream ID leaves ownership unchanged");
    response = D2B::HandleClientMessage(session, stop.message, streamIdCounter);
    Expect(response.error == ErrorCode::UnknownStream && session.state == ControlState::Streaming,
           "another wrong stream ID leaves state unchanged");
    const D2B::ParseResult matchingStop = Parse("{\"type\":\"stop_stream\",\"stream_id\":1}");
    response = D2B::HandleClientMessage(session, matchingStop.message, streamIdCounter);
    Expect(response.ok() && session.state == ControlState::Ready && !session.ownsStream && session.streamId == 0,
           "matching stop releases ownership");

    const D2B::ParseResult nulPing = Parse("{\"type\":\"ping\",\"correlation\":\"a\\u0000b\"}");
    response = D2B::HandleClientMessage(session, nulPing.message, streamIdCounter);
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
