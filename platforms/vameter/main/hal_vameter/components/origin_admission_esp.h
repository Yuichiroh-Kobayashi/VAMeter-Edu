#pragma once

#include "libs/origin_admission/origin_admission.h"
#include "libs/web_server_owner/web_server_profile.h"

#include <esp_http_server.h>

#include <cstddef>

class PsychicHttpServer;

namespace ORIGIN_ADMISSION_ESP
{
    class ServerBinding
    {
    public:
        ServerBinding();

        bool Install(PsychicHttpServer& server, const char* activeIpv4);
        void SetAdmissionReady();
        void SetAdmissionNotReady();
        bool admissionReady() const;
        const char* expectedOrigin() const;
        std::size_t expectedOriginLength() const;

    private:
        static esp_err_t OpenCallback(httpd_handle_t server, int socket);
        static int RecvOverride(httpd_handle_t server, int socket, char* buffer, std::size_t length, int flags);
        static void FreeSessionContext(void* context);
        static void KeepServerOwnedBinding(void* context);

        char _expectedOrigin[ORIGIN_ADMISSION::kMaximumOriginBytes + 1];
        ORIGIN_ADMISSION::Policy _policy;
        WEB_SERVER_PROFILE::AdmissionReadiness _admissionReadiness;
        bool _installed;

        ServerBinding(const ServerBinding&);
        ServerBinding& operator=(const ServerBinding&);
    };

    bool AcceptedWebSocket(httpd_handle_t server, int socket);
} // namespace ORIGIN_ADMISSION_ESP
