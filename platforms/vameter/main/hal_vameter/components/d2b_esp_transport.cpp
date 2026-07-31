#include "d2b_esp_transport.h"
#include "d2b_vi_producer.h"

#include "libs/d2b_vi/d2b_control.h"
#include "libs/d2b_vi/d2b_capabilities.h"
#include "libs/d2b_vi/d2b_message_buffer.h"
#include "libs/d2b_vi/d2b_session.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <cstdio>
#include <cstring>

namespace D2B_ESP
{
    namespace
    {
        static const char* kTag = "d2b-vi";
        static const char kIndex[] =
            "<!doctype html><html lang=\"en\"><meta charset=\"utf-8\"><meta name=\"viewport\" "
            "content=\"width=device-width,initial-scale=1\"><title>VAMeter-Edu D2B V/I</title>"
            "<h1>VAMeter-Edu D2B V/I</h1><p>d2b-stream/0.1 live voltage/current endpoint.</p>"
            "<ul><li><a href=\"capabilities\">capabilities</a></li><li><a href=\"status\">status</a></li></ul>"
            "</html>";

        struct Owner
        {
            httpd_handle_t server;
            int socket;
            std::uint32_t generation;
            bool active;
        };

        class Transport
        {
        public:
            Transport() : _owner(), _session(), _buffer(), _streamIdCounter(0), _generationCounter(0), _violations(0)
            {
                _owner.server = 0;
                _owner.socket = -1;
                _owner.generation = 0;
                _owner.active = false;
                D2B::CloseSession(_session);
            }

            bool open(httpd_req_t* request)
            {
                if (!originAllowed(request))
                {
                    ESP_LOGW(kTag, "rejected WebSocket origin");
                    return false;
                }

                const int socket = httpd_req_to_sockfd(request);
                if (_owner.active)
                {
                    ESP_LOGW(kTag, "rejected additional WebSocket connection");
                    return false;
                }

                ++_generationCounter;
                if (_generationCounter == 0)
                    ++_generationCounter;
                _owner.server = request->handle;
                _owner.socket = socket;
                _owner.generation = _generationCounter;
                _owner.active = true;
                _violations = 0;
                _buffer.reset();
                D2B::OpenSession(_session);
                ESP_LOGI(kTag, "WebSocket owner opened, generation=%lu", static_cast<unsigned long>(_owner.generation));
                return true;
            }

            esp_err_t receive(httpd_req_t* request)
            {
                const int socket = httpd_req_to_sockfd(request);
                if (!matches(request->handle, socket))
                    return ESP_FAIL;

                httpd_ws_frame_t frame = {};
                esp_err_t result = httpd_ws_recv_frame(request, &frame, 0);
                if (result != ESP_OK)
                    return result;

                D2B::ClientFrameType type;
                if (frame.type == HTTPD_WS_TYPE_TEXT)
                {
                    type = D2B::ClientFrameType::Text;
                    if (_buffer.assembling())
                        return rejectConsumed(request, D2B::ReassemblyResult::InvalidSequence, frame);
                }
                else if (frame.type == HTTPD_WS_TYPE_CONTINUE)
                {
                    type = D2B::ClientFrameType::Continuation;
                    if (!_buffer.assembling())
                        return rejectConsumed(request, D2B::ReassemblyResult::InvalidSequence, frame);
                }
                else
                {
                    ESP_LOGW(kTag, "rejected client WebSocket opcode=%d", static_cast<int>(frame.type));
                    return ESP_FAIL;
                }

                if (frame.len > _buffer.remaining())
                {
                    ESP_LOGW(kTag, "control frame exceeds bounded message buffer");
                    const D2B::ControlResponse response = D2B::BuildErrorResponse(D2B::ErrorCode::FrameTooLarge);
                    sendText(request, response.data, response.size);
                    return ESP_FAIL;
                }

                frame.payload = _receiveBuffer;
                result = httpd_ws_recv_frame(request, &frame, _buffer.remaining());
                if (result != ESP_OK)
                    return result;

                const D2B::ReassemblyResult reassembly =
                    _buffer.accept(type, frame.final, frame.payload, frame.len);
                if (reassembly == D2B::ReassemblyResult::NeedMore)
                    return ESP_OK;
                if (reassembly != D2B::ReassemblyResult::Complete)
                    return protocolViolation(request, D2B::ErrorCode::InvalidMessage);

                const D2B::ParseResult parsed = D2B::ParseClientMessage(_buffer.data(), _buffer.size());
                _buffer.reset();
                if (!parsed.ok())
                    return protocolViolation(request, parsed.error);

                D2B::Session proposedSession = _session;
                D2B::ControlResponse response =
                    D2B::HandleClientMessage(proposedSession, parsed.message, _streamIdCounter);
                result = sendText(request, response.data, response.size);
                if (result != ESP_OK)
                    return result;

                const bool wasStreaming = _session.state == D2B::ControlState::Streaming;
                const std::uint32_t previousStreamId = _session.streamId;
                _session = proposedSession;
                const bool isStreaming = _session.state == D2B::ControlState::Streaming;
                if (!wasStreaming && isStreaming)
                    D2B_PRODUCER::Start(_session.streamId);
                else if (wasStreaming && !isStreaming)
                    D2B_PRODUCER::Abort(previousStreamId);
                if (response.error != D2B::ErrorCode::None)
                {
                    ++_violations;
                    if (_violations >= 3)
                        return ESP_FAIL;
                }
                else
                {
                    _violations = 0;
                }
                return ESP_OK;
            }

            void close(httpd_handle_t server, int socket)
            {
                if (!matches(server, socket))
                    return;
                const std::uint32_t generation = _owner.generation;
                D2B_PRODUCER::Abort(_session.streamId);
                D2B::CloseSession(_session);
                _buffer.reset();
                _owner.server = 0;
                _owner.socket = -1;
                _owner.generation = 0;
                _owner.active = false;
                _violations = 0;
                ESP_LOGI(kTag, "WebSocket owner closed, generation=%lu", static_cast<unsigned long>(generation));
            }

            void stop(httpd_handle_t server)
            {
                if (_owner.active && _owner.server == server)
                    close(_owner.server, _owner.socket);
            }

            bool streaming() const { return _owner.active && _session.state == D2B::ControlState::Streaming; }
            unsigned connectedCount() const { return _owner.active ? 1U : 0U; }

        private:
            bool matches(httpd_handle_t server, int socket) const
            {
                return _owner.active && _owner.server == server && _owner.socket == socket && _owner.generation != 0;
            }

            bool originAllowed(httpd_req_t* request) const
            {
                const std::size_t originLength = httpd_req_get_hdr_value_len(request, "Origin");
                if (originLength == 0)
                    return true;
                const std::size_t hostLength = httpd_req_get_hdr_value_len(request, "Host");
                if (originLength >= sizeof(_originBuffer) || hostLength == 0 || hostLength >= sizeof(_hostBuffer))
                    return false;
                if (httpd_req_get_hdr_value_str(request, "Origin", _originBuffer, sizeof(_originBuffer)) != ESP_OK ||
                    httpd_req_get_hdr_value_str(request, "Host", _hostBuffer, sizeof(_hostBuffer)) != ESP_OK)
                    return false;

                char expected[sizeof(_originBuffer)];
                const int length = std::snprintf(expected, sizeof(expected), "http://%s", _hostBuffer);
                return length > 0 && static_cast<std::size_t>(length) < sizeof(expected) &&
                       std::strcmp(_originBuffer, expected) == 0;
            }

            esp_err_t rejectConsumed(httpd_req_t* request,
                                     D2B::ReassemblyResult reassembly,
                                     httpd_ws_frame_t& frame)
            {
                if (frame.len > sizeof(_receiveBuffer))
                    return ESP_FAIL;
                frame.payload = _receiveBuffer;
                const esp_err_t result = httpd_ws_recv_frame(request, &frame, sizeof(_receiveBuffer));
                if (result != ESP_OK)
                    return result;
                _buffer.reset();
                (void)reassembly;
                return protocolViolation(request, D2B::ErrorCode::InvalidMessage);
            }

            esp_err_t protocolViolation(httpd_req_t* request, D2B::ErrorCode error)
            {
                const D2B::ControlResponse response = D2B::BuildErrorResponse(error);
                const esp_err_t result = sendText(request, response.data, response.size);
                ++_violations;
                return result == ESP_OK && _violations < 3 ? ESP_OK : ESP_FAIL;
            }

            static esp_err_t sendText(httpd_req_t* request, const char* payload, std::size_t size)
            {
                httpd_ws_frame_t frame = {};
                frame.final = true;
                frame.type = HTTPD_WS_TYPE_TEXT;
                frame.payload = reinterpret_cast<std::uint8_t*>(const_cast<char*>(payload));
                frame.len = size;
                return httpd_ws_send_frame(request, &frame);
            }

            Owner _owner;
            D2B::Session _session;
            D2B::MessageBuffer _buffer;
            std::uint32_t _streamIdCounter;
            std::uint32_t _generationCounter;
            unsigned _violations;
            mutable char _originBuffer[192];
            mutable char _hostBuffer[128];
            std::uint8_t _receiveBuffer[D2B::kMaximumControlMessageSize];
        };

        Transport& GetTransport()
        {
            static Transport transport;
            return transport;
        }

        esp_err_t SendJson(httpd_req_t* request, const char* body)
        {
            httpd_resp_set_type(request, "application/json");
            httpd_resp_set_hdr(request, "Cache-Control", "no-store");
            return httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
        }

        esp_err_t IndexHandler(httpd_req_t* request)
        {
            httpd_resp_set_type(request, "text/html; charset=utf-8");
            httpd_resp_set_hdr(request, "Cache-Control", "no-store");
            return httpd_resp_send(request, kIndex, HTTPD_RESP_USE_STRLEN);
        }

        esp_err_t CapabilitiesHandler(httpd_req_t* request) { return SendJson(request, D2B::CapabilitiesJson()); }

        esp_err_t StatusHandler(httpd_req_t* request)
        {
            char response[320];
            const unsigned long long uptime =
                static_cast<unsigned long long>(esp_timer_get_time() > 9007199254740991LL ? 9007199254740991LL
                                                                                         : esp_timer_get_time());
            const int length = std::snprintf(response,
                                             sizeof(response),
                                             "{\"protocol\":\"d2b-stream\",\"version\":\"0.1\",\"state\":\"%s\","
                                             "\"connected_client_count\":%u,\"producer_drop_count\":0,"
                                             "\"output_queue_drop_count\":0,\"uptime_us\":%llu}",
                                             GetTransport().streaming() ? "streaming" : "idle",
                                             GetTransport().connectedCount(),
                                             uptime);
            if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(response))
                return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "status unavailable");
            return SendJson(request, response);
        }

        esp_err_t StreamHandler(httpd_req_t* request)
        {
            if (request->method == HTTP_GET)
                return GetTransport().open(request) ? ESP_OK : ESP_FAIL;
            return GetTransport().receive(request);
        }

        httpd_uri_t MakeUri(const char* uri, esp_err_t (*handler)(httpd_req_t*), bool websocket)
        {
            httpd_uri_t descriptor = {};
            descriptor.uri = uri;
            descriptor.method = HTTP_GET;
            descriptor.handler = handler;
            descriptor.user_ctx = 0;
            descriptor.is_websocket = websocket;
            descriptor.handle_ws_control_frames = false;
            descriptor.supported_subprotocol = 0;
            return descriptor;
        }
    } // namespace

    bool Register(httpd_handle_t server)
    {
        const httpd_uri_t routes[] = {
            MakeUri("/d2b/v0/", IndexHandler, false),
            MakeUri("/d2b/v0/capabilities", CapabilitiesHandler, false),
            MakeUri("/d2b/v0/status", StatusHandler, false),
            MakeUri("/d2b/v0/stream", StreamHandler, true),
        };
        std::size_t registered = 0;
        for (; registered < sizeof(routes) / sizeof(routes[0]); ++registered)
        {
            const esp_err_t result = httpd_register_uri_handler(server, &routes[registered]);
            if (result != ESP_OK)
            {
                ESP_LOGE(kTag, "route registration failed: %s", esp_err_to_name(result));
                while (registered != 0)
                {
                    --registered;
                    httpd_unregister_uri_handler(server, routes[registered].uri, routes[registered].method);
                }
                return false;
            }
        }
        return true;
    }

    void OnClientClosed(httpd_handle_t server, int socket) { GetTransport().close(server, socket); }

    void Stop(httpd_handle_t server) { GetTransport().stop(server); }
} // namespace D2B_ESP
