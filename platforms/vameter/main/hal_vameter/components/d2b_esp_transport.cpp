#include "d2b_esp_transport.h"
#include "d2b_vi_pipeline.h"
#include "d2b_vi_producer.h"
#include "d2b_runtime_evidence.h"
#include "origin_admission_esp.h"

#include "libs/d2b_vi/d2b_control.h"
#include "libs/d2b_vi/d2b_capabilities.h"
#include "libs/d2b_vi/d2b_message_buffer.h"
#include "libs/d2b_vi/d2b_session.h"
#include "libs/d2b_vi/d2b_stream_start_disposition.h"
#include "libs/d2b_vi/d2b_transport_runtime_state.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <cstdio>
#include <cstring>
#include <atomic>

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

        std::atomic<LIVE_SHARE_SESSION::TransportStopStatus> _localStopStatus(
            LIVE_SHARE_SESSION::TransportStopStatus::Idle);
        std::atomic<httpd_handle_t> _localStopServer(nullptr);
        std::atomic<std::uint32_t> _localStopServerGeneration(0);
        std::atomic<std::uint32_t> _registeredServerGeneration(0);
        std::atomic<std::uint32_t> _localStopRequestCounter(0);
        std::atomic<std::uint32_t> _localStopRequestId(0);
        std::atomic<std::uint32_t> _localStopWorkToken(0);

        struct Owner
        {
            httpd_handle_t server;
            int socket;
            std::uint32_t generation;
            bool active;
        };

        struct SessionPublication
        {
            class Transport* transport;
            D2B::Session session;
            D2B_PIPELINE::OwnerKey owner;
        };

        struct ControlWorkspace
        {
            D2B::ParseResult parsed;
            D2B::ControlResponse response;
            D2B::Session proposedSession;

            void reset()
            {
                parsed = {};
                response = {};
                proposedSession = {};
            }
        };

        static_assert(sizeof(ControlWorkspace) <= 2304, "control workspace exceeds Plan B static DRAM budget");

        D2B_PIPELINE::OwnerKey PipelineOwner(const Owner& owner)
        {
            const D2B_PIPELINE::OwnerKey key = {owner.server, owner.socket, owner.generation};
            return key;
        }

        bool SameOwner(const Owner& left, const Owner& right)
        {
            return left.active && right.active && left.server == right.server && left.socket == right.socket &&
                   left.generation != 0 && left.generation == right.generation;
        }

        std::uint32_t NextLocalStopRequestId()
        {
            std::uint32_t requestId = _localStopRequestCounter.fetch_add(1, std::memory_order_acq_rel) + 1U;
            if (requestId == 0)
                requestId = _localStopRequestCounter.fetch_add(1, std::memory_order_acq_rel) + 1U;
            return requestId;
        }

        class Transport
        {
        public:
            Transport()
                : _owner(),
                  _session(),
                  _buffer(),
                  _streamIdCounter(0),
                  _generationCounter(0),
                  _serverGeneration(0),
                  _localStopOwner(),
                  _localStopOwnerServerGeneration(0),
                  _localStopOwnerRequestId(0),
                  _localStopActive(false),
                  _violations(),
                  _workspace()
            {
                _owner.server = 0;
                _owner.socket = -1;
                _owner.generation = 0;
                _owner.active = false;
                resetLocalStopOwner();
                D2B::CloseSession(_session);
            }

            bool open(httpd_req_t* request)
            {
                const int socket = httpd_req_to_sockfd(request);
                if (socket < 0 || !ORIGIN_ADMISSION_ESP::AcceptedWebSocket(request->handle, socket))
                {
                    if (socket >= 0)
                        D2B_RUNTIME_EVIDENCE::LogSecurityBreadcrumb(
                            RUNTIME_EVIDENCE::Event::WebSocketConnect,
                            RUNTIME_EVIDENCE::Reason::WebSocketRejected,
                            RUNTIME_EVIDENCE::Result::Rejected,
                            0,
                            socket);
                    ESP_LOGW(kTag, "rejected WebSocket without O1-RX admission proof");
                    return false;
                }

                if (_owner.active)
                {
                    ESP_LOGW(kTag, "rejected additional WebSocket connection");
                    return false;
                }

                // A newly published owner is an exact lifecycle boundary.  A
                // gate retained for an older owner must never attach to it.
                resetLocalStopOwner();

                ++_generationCounter;
                if (_generationCounter == 0)
                    ++_generationCounter;
                _owner.server = request->handle;
                _owner.socket = socket;
                _owner.generation = _generationCounter;
                _owner.active = true;
                D2B_RUNTIME_EVIDENCE::LogSecurityBreadcrumb(RUNTIME_EVIDENCE::Event::WebSocketConnect,
                                                            RUNTIME_EVIDENCE::Reason::OwnerAcquire,
                                                            RUNTIME_EVIDENCE::Result::Accepted,
                                                            _owner.generation,
                                                            _owner.socket);
                _violations.reset();
                _workspace.reset();
                _buffer.reset();
                D2B::OpenSession(_session);
                if (!D2B_PIPELINE::Open(PipelineOwner(_owner)))
                {
                    D2B_RUNTIME_EVIDENCE::LogSecurityBreadcrumb(RUNTIME_EVIDENCE::Event::WebSocketConnect,
                                                                RUNTIME_EVIDENCE::Reason::WebSocketRejected,
                                                                RUNTIME_EVIDENCE::Result::Failed,
                                                                _owner.generation,
                                                                _owner.socket);
                    D2B::CloseSession(_session);
                    _owner.server = 0;
                    _owner.socket = -1;
                    _owner.generation = 0;
                    _owner.active = false;
                    return false;
                }
                D2B_RUNTIME_EVIDENCE::LogWebSocketConnect(PipelineOwner(_owner));
                ESP_LOGI(kTag, "WebSocket owner opened, generation=%lu", static_cast<unsigned long>(_owner.generation));
                return true;
            }

            esp_err_t receive(httpd_req_t* request)
            {
                const int socket = httpd_req_to_sockfd(request);
                if (!matches(request->handle, socket))
                    return ESP_FAIL;

                // URI handlers execute serially on the single HTTPD task, and
                // Transport admits one active WebSocket owner. Therefore this
                // singleton-lifetime workspace has exactly one writer and is
                // deliberately non-reentrant; no lock or heap allocation is
                // needed. Every invocation clears all message-dependent data.
                _workspace.reset();

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
                    D2B::BuildErrorResponseInto(D2B::ErrorCode::FrameTooLarge, _workspace.response);
                    sendText(request, _workspace.response.data, _workspace.response.size);
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
                return processCompleteMessage(request);
            }

            bool close(httpd_handle_t server,
                       int socket,
                       RUNTIME_EVIDENCE::Reason reason = RUNTIME_EVIDENCE::Reason::Disconnect)
            {
                if (!matches(server, socket))
                    return false;
                const std::uint32_t generation = _owner.generation;
                const Owner closingOwner = _owner;
                const D2B_PIPELINE::OwnerKey owner = PipelineOwner(_owner);
                D2B_PIPELINE::Close(owner, reason);
                D2B::CloseSession(_session);
                _buffer.reset();
                _owner.server = 0;
                _owner.socket = -1;
                _owner.generation = 0;
                _owner.active = false;
                _violations.reset();
                _workspace.reset();
                clearLocalStopOwnerIfMatches(closingOwner);
                D2B_RUNTIME_EVIDENCE::LogWebSocketDisconnect(owner, reason);
                ESP_LOGI(kTag, "WebSocket owner closed, generation=%lu", static_cast<unsigned long>(generation));
                return true;
            }

            void sanityCleanupAfterServerStopped(httpd_handle_t server)
            {
                D2B_PIPELINE::MarkStopSucceeded(server);
                if (_owner.active && _owner.server == server)
                {
                    D2B::CloseSession(_session);
                    _buffer.reset();
                    _owner.server = 0;
                    _owner.socket = -1;
                    _owner.generation = 0;
                    _owner.active = false;
                    _violations.reset();
                    _workspace.reset();
                }
                resetLocalStopOwner();
                _serverGeneration = 0;
            }

            void setServerGeneration(std::uint32_t generation)
            {
                resetLocalStopOwner();
                _serverGeneration = generation;
                D2B_RUNTIME_EVIDENCE::SetServerGeneration(generation);
            }

            D2B::PublicStreamingSnapshot publicSnapshot(const D2B_PIPELINE::Snapshot& pipeline,
                                                         const D2B_PRODUCER::Snapshot& producer) const
            {
                const D2B::PublicStreamingSnapshot snapshot = {
                    _owner.active,
                    _session.state == D2B::ControlState::Streaming,
                    _session.streamId,
                    reinterpret_cast<std::uintptr_t>(_owner.server),
                    _serverGeneration,
                    pipeline.initialized,
                    pipeline.ownerOpen,
                    pipeline.streamActive,
                    pipeline.stopping,
                    pipeline.taskFault,
                    pipeline.streamId,
                    producer.active,
                    producer.streamId,
                    pipeline.lifecycleAccepting,
                    pipeline.lifecycleHandleKey,
                    pipeline.lifecycleGeneration,
                };
                return snapshot;
            }

            unsigned connectedCount() const { return _owner.active ? 1U : 0U; }

            LIVE_SHARE_SESSION::TransportStopStatus beginLocalStop(httpd_handle_t server,
                                                                   std::uint32_t serverGeneration,
                                                                   std::uint32_t requestId)
            {
                if (server == nullptr || serverGeneration == 0 || requestId == 0 ||
                    _serverGeneration != serverGeneration)
                    return LIVE_SHARE_SESSION::TransportStopStatus::Complete;

                if (!_owner.active || _owner.server != server)
                {
                    resetLocalStopOwner();
                    return LIVE_SHARE_SESSION::TransportStopStatus::NoOwner;
                }

                // This code runs on the owning HTTPD task.  Admission is
                // already false, so resolve and freeze the exact current owner
                // tuple before inspecting its current stream state.
                _localStopOwner = _owner;
                _localStopOwnerServerGeneration = serverGeneration;
                _localStopOwnerRequestId = requestId;
                _localStopActive = true;

                if (_session.state != D2B::ControlState::Streaming)
                {
                    return httpd_sess_trigger_close(_owner.server, _owner.socket) == ESP_OK
                               ? LIVE_SHARE_SESSION::TransportStopStatus::OwnerClosing
                               : LIVE_SHARE_SESSION::TransportStopStatus::Failed;
                }

                const std::uint32_t streamId = _session.streamId;
                char stoppedResponse[128];
                const int length = std::snprintf(stoppedResponse,
                                                 sizeof(stoppedResponse),
                                                 "{\"type\":\"stream_stopped\",\"stream_id\":%lu,"
                                                 "\"reason\":\"device stop\"}",
                                                 static_cast<unsigned long>(streamId));
                if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(stoppedResponse) ||
                    !D2B_PIPELINE::RequestOrderlyStop(PipelineOwner(_owner),
                                                      streamId,
                                                      stoppedResponse,
                                                      static_cast<std::size_t>(length)))
                    return LIVE_SHARE_SESSION::TransportStopStatus::Failed;

                _session.state = D2B::ControlState::Ready;
                _session.ownsStream = false;
                _session.streamId = 0;
                return LIVE_SHARE_SESSION::TransportStopStatus::ActiveStreamStopping;
            }

            LIVE_SHARE_SESSION::TransportStopStatus pollLocalStop(httpd_handle_t server,
                                                                  std::uint32_t serverGeneration,
                                                                  std::uint32_t requestId)
            {
                if (!localStopOwnerMatches(server, serverGeneration, requestId))
                    return LIVE_SHARE_SESSION::TransportStopStatus::Complete;

                if (!SameOwner(_owner, _localStopOwner))
                {
                    resetLocalStopOwner();
                    return LIVE_SHARE_SESSION::TransportStopStatus::Complete;
                }

                const D2B_PIPELINE::Snapshot pipeline = D2B_PIPELINE::GetSnapshot();
                if (pipeline.streamActive || pipeline.stopping)
                    return LIVE_SHARE_SESSION::TransportStopStatus::ActiveStreamStopping;

                return httpd_sess_trigger_close(_owner.server, _owner.socket) == ESP_OK
                           ? LIVE_SHARE_SESSION::TransportStopStatus::OwnerClosing
                           : LIVE_SHARE_SESSION::TransportStopStatus::Failed;
            }

        private:
            esp_err_t processCompleteMessage(httpd_req_t* request)
            {
                D2B::ParseClientMessageInto(_buffer.data(), _buffer.size(), _workspace.parsed);
                _buffer.reset();
                if (!_workspace.parsed.ok())
                    return protocolViolation(request, _workspace.parsed.error);

                // Local Stop Sharing owns this exact admitted owner until the
                // owner actually closes.  In particular this remains armed
                // after the pipeline clears StopPending, so READY->STREAMING
                // cannot be resurrected in the orderly-stop/close window.
                if (localStopGatesCurrentOwner())
                {
                    ESP_LOGW(kTag, "closing locally stopping WebSocket owner after client control");
                    return ESP_FAIL;
                }

                if (D2B_PIPELINE::StopPending(PipelineOwner(_owner)))
                    return protocolViolation(request, D2B::ErrorCode::InvalidState);

                _workspace.proposedSession = _session;
                D2B::HandleClientMessageInto(_workspace.proposedSession,
                                             _workspace.parsed.message,
                                             _streamIdCounter,
                                             _workspace.response);
                const bool wasStreaming = _session.state == D2B::ControlState::Streaming;
                const std::uint32_t previousStreamId = _session.streamId;
                const bool isOrderlyStop = wasStreaming && _workspace.response.ok() &&
                                           _workspace.parsed.message.type == D2B::ClientMessageType::StopStream &&
                                           _workspace.proposedSession.state == D2B::ControlState::Ready;
                if (isOrderlyStop)
                {
                    // RequestOrderlyStop copies these bytes into pipeline-owned
                    // storage before it returns; no workspace pointer escapes.
                    if (!D2B_PIPELINE::RequestOrderlyStop(PipelineOwner(_owner),
                                                          previousStreamId,
                                                          _workspace.response.data,
                                                          _workspace.response.size))
                        return ESP_FAIL;
                }
                else
                {
                    // httpd_ws_send_frame is synchronous for request-context
                    // sends, so the payload remains valid through its return.
                    const esp_err_t result = sendText(request, _workspace.response.data, _workspace.response.size);
                    if (result != ESP_OK)
                        return result;
                }

                const bool isStreaming = _workspace.proposedSession.state == D2B::ControlState::Streaming;
                if (!wasStreaming && isStreaming)
                {
                    SessionPublication publication = {this, _workspace.proposedSession, PipelineOwner(_owner)};
                    // StartStream invokes PublishSession synchronously before
                    // returning, so publication and proposedSession stay live.
                    const bool pipelineStarted = D2B_PIPELINE::StartStream(PipelineOwner(_owner),
                                                                             _workspace.proposedSession.streamId,
                                                                             &Transport::PublishSession,
                                                                             &publication);
                    if (D2B::DecideStreamStartDisposition(pipelineStarted) ==
                        D2B::StreamStartDisposition::CloseConnection)
                        return ESP_FAIL;
                }
                else
                {
                    _session = _workspace.proposedSession;
                }
                if (_workspace.response.error != D2B::ErrorCode::None)
                {
                    if (_violations.recordFailure())
                        return ESP_FAIL;
                }
                else
                {
                    _violations.reset();
                }
                return ESP_OK;
            }

            bool matches(httpd_handle_t server, int socket) const
            {
                return _owner.active && _owner.server == server && _owner.socket == socket && _owner.generation != 0;
            }

            bool localStopGatesCurrentOwner() const
            {
                return _localStopActive && _localStopOwnerServerGeneration == _serverGeneration &&
                       SameOwner(_owner, _localStopOwner);
            }

            bool localStopOwnerMatches(httpd_handle_t server,
                                       std::uint32_t serverGeneration,
                                       std::uint32_t requestId) const
            {
                return _localStopActive && server != nullptr && serverGeneration != 0 && requestId != 0 &&
                       _localStopOwner.server == server &&
                       _localStopOwnerServerGeneration == serverGeneration &&
                       _localStopOwnerRequestId == requestId;
            }

            void clearLocalStopOwnerIfMatches(const Owner& owner)
            {
                if (_localStopActive && SameOwner(_localStopOwner, owner))
                    resetLocalStopOwner();
            }

            void resetLocalStopOwner()
            {
                _localStopOwner.server = nullptr;
                _localStopOwner.socket = -1;
                _localStopOwner.generation = 0;
                _localStopOwner.active = false;
                _localStopOwnerServerGeneration = 0;
                _localStopOwnerRequestId = 0;
                _localStopActive = false;
            }

            static bool PublishSession(void* context)
            {
                SessionPublication* publication = static_cast<SessionPublication*>(context);
                if (publication == nullptr || publication->transport == nullptr)
                    return false;
                Transport* transport = publication->transport;
                if (!transport->matches(publication->owner.server, publication->owner.socket) ||
                    transport->_owner.generation != publication->owner.generation)
                    return false;
                transport->_session = publication->session;
                return true;
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
                D2B::BuildErrorResponseInto(error, _workspace.response);
                const esp_err_t result = sendText(request, _workspace.response.data, _workspace.response.size);
                const bool closeConnection = _violations.recordFailure();
                return result == ESP_OK && !closeConnection ? ESP_OK : ESP_FAIL;
            }

            esp_err_t sendText(httpd_req_t* request, const char* payload, std::size_t size)
            {
                const int socket = httpd_req_to_sockfd(request);
                if (!matches(request->handle, socket))
                    return ESP_FAIL;
                if (payload == nullptr || size == 0)
                    return ESP_ERR_INVALID_ARG;
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
            std::uint32_t _serverGeneration;
            Owner _localStopOwner;
            std::uint32_t _localStopOwnerServerGeneration;
            std::uint32_t _localStopOwnerRequestId;
            bool _localStopActive;
            D2B::ControlViolationState _violations;
            ControlWorkspace _workspace;
            std::uint8_t _receiveBuffer[D2B::kMaximumControlMessageSize];
        };

        Transport& GetTransport()
        {
            static Transport transport;
            return transport;
        }

        void ReleaseLocalStopWorkToken(std::uint32_t requestId)
        {
            std::uint32_t expected = requestId;
            (void)_localStopWorkToken.compare_exchange_strong(expected, 0, std::memory_order_acq_rel);
        }

        void LocalStopWork(void* context)
        {
            const std::uint32_t requestId =
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(context));
            if (requestId == 0 || requestId != _localStopRequestId.load(std::memory_order_acquire))
            {
                ReleaseLocalStopWorkToken(requestId);
                return;
            }

            const httpd_handle_t server = _localStopServer.load(std::memory_order_acquire);
            const std::uint32_t serverGeneration =
                _localStopServerGeneration.load(std::memory_order_acquire);
            const LIVE_SHARE_SESSION::TransportStopStatus current =
                _localStopStatus.load(std::memory_order_acquire);
            LIVE_SHARE_SESSION::TransportStopStatus next = current;
            if (current == LIVE_SHARE_SESSION::TransportStopStatus::Queued)
                next = GetTransport().beginLocalStop(server, serverGeneration, requestId);
            else if (current == LIVE_SHARE_SESSION::TransportStopStatus::ActiveStreamStopping)
                next = GetTransport().pollLocalStop(server, serverGeneration, requestId);

            if (requestId == _localStopRequestId.load(std::memory_order_acquire))
                _localStopStatus.store(next, std::memory_order_release);
            ReleaseLocalStopWorkToken(requestId);
        }

        bool QueueLocalStopWork(httpd_handle_t server, std::uint32_t requestId)
        {
            if (server == nullptr || requestId == 0 ||
                requestId != _localStopRequestId.load(std::memory_order_acquire))
                return false;

            std::uint32_t expected = 0;
            if (!_localStopWorkToken.compare_exchange_strong(expected, requestId, std::memory_order_acq_rel))
                return expected == requestId;

            void* const context = reinterpret_cast<void*>(static_cast<std::uintptr_t>(requestId));
            if (httpd_queue_work(server, LocalStopWork, context) != ESP_OK)
            {
                ReleaseLocalStopWorkToken(requestId);
                _localStopStatus.store(LIVE_SHARE_SESSION::TransportStopStatus::Failed,
                                       std::memory_order_release);
                return false;
            }
            return true;
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
            char response[384];
            const std::uint64_t maximumSafeInteger = 9007199254740991ULL;
            const std::uint64_t rawUptime = static_cast<std::uint64_t>(esp_timer_get_time());
            const D2B_PIPELINE::Snapshot pipeline = D2B_PIPELINE::GetSnapshot();
            const D2B_PRODUCER::Snapshot producer = D2B_PRODUCER::GetSnapshot();
            const D2B::PublicStreamingSnapshot transport = GetTransport().publicSnapshot(pipeline, producer);
            const std::uint64_t producerDrops =
                producer.producerDropCount > maximumSafeInteger ? maximumSafeInteger : producer.producerDropCount;
            const std::uint64_t outputDrops = pipeline.outputQueueDropCount > maximumSafeInteger
                                                  ? maximumSafeInteger
                                                  : pipeline.outputQueueDropCount;
            const std::uint64_t queued = static_cast<std::uint64_t>(producer.queuedSampleCount) +
                                         pipeline.queuedOutputFrames;
            const unsigned long long uptime =
                static_cast<unsigned long long>(rawUptime > maximumSafeInteger ? maximumSafeInteger : rawUptime);
            const int length = std::snprintf(response,
                                             sizeof(response),
                                             "{\"protocol\":\"d2b-stream\",\"version\":\"0.1\",\"state\":\"%s\","
                                             "\"connected_client_count\":%u,\"producer_drop_count\":%llu,"
                                             "\"output_queue_drop_count\":%llu,\"queued_sample_count\":%llu,"
                                             "\"uptime_us\":%llu}",
                                             D2B::IsPublicStreaming(transport) ? "streaming" : "idle",
                                             GetTransport().connectedCount(),
                                             static_cast<unsigned long long>(producerDrops),
                                             static_cast<unsigned long long>(outputDrops),
                                             static_cast<unsigned long long>(queued),
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

    bool Register(httpd_handle_t server, std::uint32_t generation)
    {
        if (!D2B_PIPELINE::Initialize())
        {
            ESP_LOGE(kTag, "failed to initialize bounded encoder/TX tasks");
            return false;
        }
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
        if (!D2B_PIPELINE::CommitServerRunning(server, generation))
        {
            ESP_LOGE(kTag, "D2B lifecycle commit failed after route registration");
            while (registered != 0)
            {
                --registered;
                httpd_unregister_uri_handler(server, routes[registered].uri, routes[registered].method);
            }
            return false;
        }
        return true;
    }

    void SetServerGeneration(std::uint32_t generation)
    {
        _registeredServerGeneration.store(generation, std::memory_order_release);
        GetTransport().setServerGeneration(generation);
    }

    bool RequestLocalStop(httpd_handle_t server)
    {
        const std::uint32_t serverGeneration = _registeredServerGeneration.load(std::memory_order_acquire);
        if (server == nullptr || serverGeneration == 0)
            return false;

        const std::uint32_t requestId = NextLocalStopRequestId();
        _localStopServer.store(server, std::memory_order_release);
        _localStopServerGeneration.store(serverGeneration, std::memory_order_release);
        _localStopStatus.store(LIVE_SHARE_SESSION::TransportStopStatus::Queued, std::memory_order_release);
        _localStopRequestId.store(requestId, std::memory_order_release);
        return QueueLocalStopWork(server, requestId);
    }

    LIVE_SHARE_SESSION::TransportStopStatus PollLocalStop(httpd_handle_t server)
    {
        const LIVE_SHARE_SESSION::TransportStopStatus status =
            _localStopStatus.load(std::memory_order_acquire);
        const std::uint32_t requestId = _localStopRequestId.load(std::memory_order_acquire);
        if (_localStopServer.load(std::memory_order_acquire) != server)
            return LIVE_SHARE_SESSION::TransportStopStatus::Complete;
        if (status == LIVE_SHARE_SESSION::TransportStopStatus::Queued ||
            status == LIVE_SHARE_SESSION::TransportStopStatus::ActiveStreamStopping)
            (void)QueueLocalStopWork(server, requestId);
        return _localStopStatus.load(std::memory_order_acquire);
    }

    void OnClientClosed(httpd_handle_t server, int socket)
    {
        const bool closed = GetTransport().close(server, socket, RUNTIME_EVIDENCE::Reason::Disconnect);
        const LIVE_SHARE_SESSION::TransportStopStatus status =
            _localStopStatus.load(std::memory_order_acquire);
        if (closed && _localStopServer.load(std::memory_order_acquire) == server &&
            (status == LIVE_SHARE_SESSION::TransportStopStatus::Queued ||
             status == LIVE_SHARE_SESSION::TransportStopStatus::OwnerClosing ||
             status == LIVE_SHARE_SESSION::TransportStopStatus::ActiveStreamStopping))
            _localStopStatus.store(LIVE_SHARE_SESSION::TransportStopStatus::Complete, std::memory_order_release);
    }

    void PrepareServerStop(httpd_handle_t server)
    {
        D2B_PIPELINE::StopServer(server, RUNTIME_EVIDENCE::Reason::ServerStop);
    }

    void ServerStopFailed(httpd_handle_t server) { D2B_PIPELINE::MarkStopFailed(server); }

    void AfterServerStopped(httpd_handle_t server)
    {
        GetTransport().sanityCleanupAfterServerStopped(server);
        const std::uint32_t invalidatedRequestId = NextLocalStopRequestId();
        _localStopServer.store(nullptr, std::memory_order_release);
        _localStopServerGeneration.store(0, std::memory_order_release);
        _registeredServerGeneration.store(0, std::memory_order_release);
        _localStopRequestId.store(invalidatedRequestId, std::memory_order_release);
        _localStopWorkToken.store(0, std::memory_order_release);
        _localStopStatus.store(LIVE_SHARE_SESSION::TransportStopStatus::Complete, std::memory_order_release);
    }
} // namespace D2B_ESP
