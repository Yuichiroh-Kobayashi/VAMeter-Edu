#pragma once

#include <esp_http_server.h>

#include <cstdint>

namespace D2B_ESP
{
    bool Register(httpd_handle_t server, std::uint32_t generation);
    void SetServerGeneration(std::uint32_t generation);
    void OnClientClosed(httpd_handle_t server, int socket);
    void PrepareServerStop(httpd_handle_t server);
    void ServerStopFailed(httpd_handle_t server);
    void AfterServerStopped(httpd_handle_t server);
} // namespace D2B_ESP
