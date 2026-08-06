#include "d2b_session.h"

#include <cstdio>
#include <cstring>

namespace D2B
{
    namespace
    {
        static const std::uint64_t kMaximumSafeJsonInteger = 9007199254740991ULL;

        class ResponseWriter
        {
        public:
            explicit ResponseWriter(ControlResponse& response) : _response(response), _failed(false)
            {
                _response.error = ErrorCode::None;
                _response.size = 0;
                _response.data[0] = '\0';
            }

            bool append(const char* value)
            {
                return append(value, std::strlen(value));
            }

            bool append(const char* value, std::size_t size)
            {
                if (_failed || _response.size + size + 1 > sizeof(_response.data))
                {
                    _failed = true;
                    return false;
                }
                std::memcpy(_response.data + _response.size, value, size);
                _response.size += size;
                _response.data[_response.size] = '\0';
                return true;
            }

            bool appendUnsigned(std::uint64_t value)
            {
                char buffer[24];
                const int length = std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
                return length > 0 && append(buffer, static_cast<std::size_t>(length));
            }

            bool appendJsonString(const char* value, std::size_t size)
            {
                if (!append("\""))
                    return false;
                for (std::size_t index = 0; index < size; ++index)
                {
                    const unsigned char character = static_cast<unsigned char>(value[index]);
                    switch (character)
                    {
                        case '"': if (!append("\\\"")) return false; break;
                        case '\\': if (!append("\\\\")) return false; break;
                        case '\b': if (!append("\\b")) return false; break;
                        case '\f': if (!append("\\f")) return false; break;
                        case '\n': if (!append("\\n")) return false; break;
                        case '\r': if (!append("\\r")) return false; break;
                        case '\t': if (!append("\\t")) return false; break;
                        default:
                            if (character < 0x20)
                            {
                                char escaped[7];
                                const int length = std::snprintf(escaped, sizeof(escaped), "\\u%04x", character);
                                if (length != 6 || !append(escaped, 6))
                                    return false;
                            }
                            else if (!append(value + index, 1))
                            {
                                return false;
                            }
                            break;
                    }
                }
                return append("\"");
            }

            bool finish()
            {
                if (_failed)
                {
                    _response.error = ErrorCode::InternalError;
                    _response.size = 0;
                    _response.data[0] = '\0';
                    return false;
                }
                return true;
            }

        private:
            ControlResponse& _response;
            bool _failed;
        };

        const char* ErrorMessage(ErrorCode error)
        {
            switch (error)
            {
                case ErrorCode::Busy: return "stream already owned";
                case ErrorCode::Unauthorized: return "authentication failed";
                case ErrorCode::UnknownStream: return "stream is not supported";
                case ErrorCode::UnsupportedVersion: return "version is not supported";
                case ErrorCode::UnsupportedProfile: return "profile is not supported";
                case ErrorCode::UnsupportedParameters: return "parameters are not supported";
                case ErrorCode::InvalidMessage: return "control message is invalid";
                case ErrorCode::InvalidState: return "message is invalid in the current state";
                case ErrorCode::FrameTooLarge: return "control message is too large";
                case ErrorCode::InternalError: return "internal error";
                case ErrorCode::None: return "";
            }
            return "internal error";
        }

        std::uint64_t SaturateSafeInteger(std::uint64_t value)
        {
            return value > kMaximumSafeJsonInteger ? kMaximumSafeJsonInteger : value;
        }

        std::uint32_t NextStreamId(std::uint32_t& counter)
        {
            ++counter;
            if (counter == 0)
                ++counter;
            return counter;
        }
    } // namespace

    void OpenSession(Session& session)
    {
        session.state = ControlState::Connected;
        session.ownsStream = false;
        session.streamId = 0;
    }

    void CloseSession(Session& session)
    {
        session.state = ControlState::Closed;
        session.ownsStream = false;
        session.streamId = 0;
    }

    void BuildErrorResponseInto(ErrorCode error, ControlResponse& output)
    {
        output = {};
        ResponseWriter writer(output);
        writer.append("{\"type\":\"error\",\"code\":\"");
        writer.append(ErrorCodeName(error));
        writer.append("\",\"message\":\"");
        writer.append(ErrorMessage(error));
        writer.append("\",\"recoverable\":true}");
        writer.finish();
        if (output.error == ErrorCode::None)
            output.error = error;
    }

    ControlResponse BuildErrorResponse(ErrorCode error)
    {
        ControlResponse response = {};
        BuildErrorResponseInto(error, response);
        return response;
    }

    void HandleClientMessageInto(Session& session,
                                 const ClientMessage& message,
                                 std::uint32_t& streamIdCounter,
                                 ControlResponse& output)
    {
        output = {};
        const ErrorCode stateError = ValidateClientMessageState(message, session.state, session.ownsStream);
        if (stateError != ErrorCode::None)
        {
            BuildErrorResponseInto(stateError, output);
            return;
        }

        ResponseWriter writer(output);
        if (message.type == ClientMessageType::Hello)
        {
            if (!message.supportsVersion01)
            {
                BuildErrorResponseInto(ErrorCode::UnsupportedVersion, output);
                return;
            }
            writer.append("{\"type\":\"welcome\",\"protocol\":\"d2b-stream\",\"version\":\"0.1\","
                          "\"max_control_message_size\":2048,\"max_binary_frame_size\":48,\"session_state\":\"ready\","
                          "\"server_name\":\"VAMeter-Edu\"}");
            if (writer.finish())
                session.state = ControlState::Ready;
            return;
        }

        if (message.type == ClientMessageType::StartStream)
        {
            if (std::strcmp(message.stream, "live-vi") != 0)
            {
                BuildErrorResponseInto(ErrorCode::UnknownStream, output);
                return;
            }
            if (message.profile != Profile::ViMeasurement)
            {
                BuildErrorResponseInto(ErrorCode::UnsupportedProfile, output);
                return;
            }
            const std::uint32_t streamId = NextStreamId(streamIdCounter);
            writer.append("{\"type\":\"stream_started\",\"stream\":\"live-vi\",\"profile\":\"vi-measurement\","
                          "\"parameters\":{\"sample_format\":\"vi-f32le\",\"channel_count\":2,\"channel_mask\":3,"
                          "\"sample_rate\":{\"numerator\":0,\"denominator\":0}},\"stream_id\":");
            writer.appendUnsigned(streamId);
            writer.append("}");
            if (writer.finish())
            {
                session.state = ControlState::Streaming;
                session.ownsStream = true;
                session.streamId = streamId;
            }
            return;
        }

        if (message.type == ClientMessageType::StopStream)
        {
            if (message.hasStreamId && message.streamId != session.streamId)
            {
                BuildErrorResponseInto(ErrorCode::UnknownStream, output);
                return;
            }
            const std::uint32_t stoppedId = session.streamId;
            writer.append("{\"type\":\"stream_stopped\",\"stream_id\":");
            writer.appendUnsigned(stoppedId);
            writer.append(",\"reason\":\"client request\"}");
            if (writer.finish())
            {
                session.state = ControlState::Ready;
                session.ownsStream = false;
                session.streamId = 0;
            }
            return;
        }

        writer.append("{\"type\":\"pong\",\"correlation\":");
        writer.appendJsonString(message.correlation, message.correlationBytes);
        writer.append("}");
        writer.finish();
    }

    ControlResponse HandleClientMessage(Session& session, const ClientMessage& message, std::uint32_t& streamIdCounter)
    {
        ControlResponse response = {};
        HandleClientMessageInto(session, message, streamIdCounter, response);
        return response;
    }

    ControlResponse BuildStatusResponse(const Session& session,
                                        std::uint64_t uptimeUs,
                                        std::uint64_t producerDropCount,
                                        std::uint64_t outputQueueDropCount,
                                        std::uint32_t queuedSampleCount)
    {
        ControlResponse response = {};
        ResponseWriter writer(response);
        const bool streaming = session.state == ControlState::Streaming;
        writer.append("{\"type\":\"status\",\"state\":\"");
        writer.append(streaming ? "streaming" : "idle");
        writer.append("\"");
        if (streaming)
        {
            writer.append(",\"active_stream_id\":");
            writer.appendUnsigned(session.streamId);
        }
        writer.append(",\"connected_client_count\":1,\"producer_drop_count\":");
        writer.appendUnsigned(SaturateSafeInteger(producerDropCount));
        writer.append(",\"output_queue_drop_count\":");
        writer.appendUnsigned(SaturateSafeInteger(outputQueueDropCount));
        writer.append(",\"queued_sample_count\":");
        writer.appendUnsigned(queuedSampleCount);
        writer.append(",\"source_paused\":false,\"uptime_us\":");
        writer.appendUnsigned(SaturateSafeInteger(uptimeUs));
        writer.append("}");
        writer.finish();
        return response;
    }
} // namespace D2B
