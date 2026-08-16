#pragma once

#include <esp_http_server.h>

#include <cstdint>

#include "libs/live_share_session/live_share_session.h"

namespace D2B_ESP
{
    bool Register(httpd_handle_t server, std::uint32_t generation);
    void SetServerGeneration(std::uint32_t generation);
    bool RequestLocalStop(httpd_handle_t server);
    LIVE_SHARE_SESSION::TransportStopStatus PollLocalStop(httpd_handle_t server);
    void OnClientClosed(httpd_handle_t server, int socket);
    void PrepareServerStop(httpd_handle_t server);
    void ServerStopFailed(httpd_handle_t server);
    void AfterServerStopped(httpd_handle_t server);
} // namespace D2B_ESP
