#pragma once

#include <esp_http_server.h>

#include <cstdint>

namespace D2B_ESP
{
    bool Register(httpd_handle_t server);
    void SetServerGeneration(std::uint32_t generation);
    void OnClientClosed(httpd_handle_t server, int socket);
    void Stop(httpd_handle_t server);
} // namespace D2B_ESP
