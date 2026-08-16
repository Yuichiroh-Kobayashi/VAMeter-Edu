#include "origin_admission_esp.h"

#include "d2b_runtime_evidence.h"

#include <PsychicHttp.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>

namespace ORIGIN_ADMISSION_ESP
{
    namespace
    {
        struct SessionContext
        {
            ORIGIN_ADMISSION::State scanner;
            bool acceptedWebSocket;
        };

        int MapRecvFailure()
        {
            switch (errno)
            {
            case EAGAIN:
            case EINTR:
                return HTTPD_SOCK_ERR_TIMEOUT;
            case EINVAL:
            case EBADF:
            case EFAULT:
            case ENOTSOCK:
                return HTTPD_SOCK_ERR_INVALID;
            default:
                return HTTPD_SOCK_ERR_FAIL;
            }
        }

        SessionContext* GetSessionContext(httpd_handle_t server, int socket)
        {
            return static_cast<SessionContext*>(httpd_sess_get_transport_ctx(server, socket));
        }

        RUNTIME_EVIDENCE::Reason RejectionReason(ORIGIN_ADMISSION::RejectReason reason)
        {
            switch (reason)
            {
            case ORIGIN_ADMISSION::RejectReason::MissingOrigin:
            case ORIGIN_ADMISSION::RejectReason::EmptyOrigin:
            case ORIGIN_ADMISSION::RejectReason::NullOrigin:
            case ORIGIN_ADMISSION::RejectReason::MalformedOrigin:
            case ORIGIN_ADMISSION::RejectReason::DuplicateOrigin:
            case ORIGIN_ADMISSION::RejectReason::OriginComma:
            case ORIGIN_ADMISSION::RejectReason::OriginTooLong:
            case ORIGIN_ADMISSION::RejectReason::OriginMismatch:
                return RUNTIME_EVIDENCE::Reason::OriginRejected;
            default:
                return RUNTIME_EVIDENCE::Reason::WebSocketRejected;
            }
        }
    } // namespace

    ServerBinding::ServerBinding() : _expectedOrigin(), _policy(), _admissionReadiness(), _installed(false)
    {
        _policy.expected_origin = _expectedOrigin;
        _policy.expected_origin_length = 0;
    }

    bool ServerBinding::Install(PsychicHttpServer& server, const char* activeIpv4)
    {
        SetAdmissionNotReady();
        if (_installed || server.server != nullptr || activeIpv4 == nullptr ||
            server.config.global_transport_ctx != nullptr)
            return false;

        in_addr parsedAddress = {};
        if (inet_pton(AF_INET, activeIpv4, &parsedAddress) != 1)
            return false;

        char canonicalIpv4[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &parsedAddress, canonicalIpv4, sizeof(canonicalIpv4)) == nullptr)
            return false;

        const int length = std::snprintf(_expectedOrigin, sizeof(_expectedOrigin), "http://%s", canonicalIpv4);
        if (length <= 0 || static_cast<std::size_t>(length) > ORIGIN_ADMISSION::kMaximumOriginBytes ||
            static_cast<std::size_t>(length) >= sizeof(_expectedOrigin))
            return false;

        _policy.expected_origin_length = static_cast<std::size_t>(length);
        ORIGIN_ADMISSION::State validation = {};
        if (!ORIGIN_ADMISSION::Initialize(validation, _policy))
        {
            _policy.expected_origin_length = 0;
            _expectedOrigin[0] = '\0';
            return false;
        }

        server.config.global_transport_ctx = this;
        server.config.global_transport_ctx_free_fn = KeepServerOwnedBinding;
        server.config.open_fn = OpenCallback;
        _installed = true;
        return true;
    }

    void ServerBinding::SetAdmissionReady() { _admissionReadiness.setReady(); }

    void ServerBinding::SetAdmissionNotReady() { _admissionReadiness.reset(); }

    bool ServerBinding::admissionReady() const { return _admissionReadiness.isReady(); }

    const char* ServerBinding::expectedOrigin() const { return _expectedOrigin; }

    std::size_t ServerBinding::expectedOriginLength() const { return _policy.expected_origin_length; }

    esp_err_t ServerBinding::OpenCallback(httpd_handle_t server, int socket)
    {
        const esp_err_t psychicOpen = PsychicHttpServer::openCallback(server, socket);
        if (psychicOpen != ESP_OK)
            return psychicOpen;

        ServerBinding* binding = static_cast<ServerBinding*>(httpd_get_global_transport_ctx(server));
        if (binding == nullptr || !binding->_installed)
            return ESP_ERR_INVALID_STATE;
        if (!binding->_admissionReadiness.allowsSessionContextCreation())
            return ESP_FAIL;

        SessionContext* context = new (std::nothrow) SessionContext();
        if (context == nullptr)
            return ESP_ERR_NO_MEM;
        context->acceptedWebSocket = false;
        if (!ORIGIN_ADMISSION::Initialize(context->scanner, binding->_policy))
        {
            delete context;
            return ESP_ERR_INVALID_STATE;
        }

        httpd_sess_set_transport_ctx(server, socket, context, FreeSessionContext);
        if (GetSessionContext(server, socket) != context)
        {
            delete context;
            return ESP_ERR_INVALID_STATE;
        }

        const esp_err_t overrideResult = httpd_sess_set_recv_override(server, socket, RecvOverride);
        if (overrideResult != ESP_OK)
        {
            httpd_sess_set_transport_ctx(server, socket, nullptr, nullptr);
            return overrideResult;
        }
        return ESP_OK;
    }

    int ServerBinding::RecvOverride(httpd_handle_t server,
                                    int socket,
                                    char* buffer,
                                    std::size_t length,
                                    int flags)
    {
        if (buffer == nullptr)
            return HTTPD_SOCK_ERR_INVALID;

        const int received = recv(socket, buffer, length, flags);
        if (received < 0)
            return MapRecvFailure();
        if (received == 0)
            return 0;

        SessionContext* context = GetSessionContext(server, socket);
        if (context == nullptr)
            return HTTPD_SOCK_ERR_FAIL;

        const ORIGIN_ADMISSION::Result result = ORIGIN_ADMISSION::Consume(
            context->scanner,
            reinterpret_cast<const std::uint8_t*>(buffer),
            static_cast<std::size_t>(received));
        if (result.decision == ORIGIN_ADMISSION::Decision::Rejected)
        {
            D2B_RUNTIME_EVIDENCE::LogSecurityBreadcrumb(RUNTIME_EVIDENCE::Event::ServerRequest,
                                                        RejectionReason(result.reason),
                                                        RUNTIME_EVIDENCE::Result::Rejected,
                                                        0,
                                                        socket);
            return HTTPD_SOCK_ERR_FAIL;
        }
        if (result.decision == ORIGIN_ADMISSION::Decision::AcceptedWebSocket && !context->acceptedWebSocket)
        {
            context->acceptedWebSocket = true;
            D2B_RUNTIME_EVIDENCE::LogSecurityBreadcrumb(RUNTIME_EVIDENCE::Event::ServerRequest,
                                                        RUNTIME_EVIDENCE::Reason::WebSocketAccepted,
                                                        RUNTIME_EVIDENCE::Result::Accepted,
                                                        0,
                                                        socket);
        }

        // The scanner is an admission observer. Every accepted raw receive
        // block, including a WebSocket first-frame tail, remains untouched.
        return received;
    }

    void ServerBinding::FreeSessionContext(void* context) { delete static_cast<SessionContext*>(context); }

    void ServerBinding::KeepServerOwnedBinding(void* context) { (void)context; }

    bool AcceptedWebSocket(httpd_handle_t server, int socket)
    {
        const SessionContext* context = GetSessionContext(server, socket);
        return context != nullptr && context->acceptedWebSocket;
    }
} // namespace ORIGIN_ADMISSION_ESP
