#pragma once

#include <esp_http_server.h>

struct WebPagePool_t;

namespace VIEWER_HTTP_ROUTES
{
    bool HasExpectedAssetIdentity(const WebPagePool_t* webPages);
    bool Register(httpd_handle_t server, const WebPagePool_t* webPages);
} // namespace VIEWER_HTTP_ROUTES
