#pragma once

#include <esp_http_server.h>
#include "libs/viewer_asset_contract/viewer_asset_contract.h"

struct WebPagePool_t;

namespace VIEWER_HTTP_ROUTES
{
    bool HasExpectedAssetIdentity(const WebPagePool_t* webPages);
    bool Register(httpd_handle_t server,
                  const WebPagePool_t* webPages,
                  VIEWER_ASSET_CONTRACT::DisplayProfile displayProfile);
    void Reset();
} // namespace VIEWER_HTTP_ROUTES
