#include "viewer_http_routes.h"

#include "assets/web/types.h"
#include "libs/viewer_asset_contract/viewer_asset_contract.h"

#include <esp_log.h>

#include <cstddef>

namespace VIEWER_HTTP_ROUTES
{
    namespace
    {
        const char* const kTag = "VIEWER_HTTP";
        const WebPagePool_t* g_webPages = nullptr;
        httpd_uri_t g_routes[VIEWER_ASSET_CONTRACT::kViewerRouteCount] = {};

        esp_err_t SetCacheControl(httpd_req_t* request, const char* value)
        {
            return httpd_resp_set_hdr(request, "Cache-Control", value);
        }

        esp_err_t SendFixed(httpd_req_t* request,
                            const char* mime,
                            const char* contentEncoding,
                            const char* cacheControl,
                            const std::uint8_t* bytes,
                            std::size_t size)
        {
            esp_err_t result = httpd_resp_set_type(request, mime);
            if (result != ESP_OK)
                return result;
            if (contentEncoding != nullptr)
            {
                result = httpd_resp_set_hdr(request, "Content-Encoding", contentEncoding);
                if (result != ESP_OK)
                    return result;
            }
            result = SetCacheControl(request, cacheControl);
            if (result != ESP_OK)
                return result;
            return httpd_resp_send(request, reinterpret_cast<const char*>(bytes), size);
        }

        esp_err_t RootHandler(httpd_req_t* request)
        {
            esp_err_t result = httpd_resp_set_status(request, "302 Found");
            if (result != ESP_OK)
                return result;
            result = httpd_resp_set_hdr(request, "Location", VIEWER_ASSET_CONTRACT::kViewerRoute);
            if (result != ESP_OK)
                return result;
            result = SetCacheControl(request, VIEWER_ASSET_CONTRACT::kNoStoreCacheControl);
            if (result != ESP_OK)
                return result;
            return httpd_resp_send(request, "", 0U);
        }

        esp_err_t IndexHandler(httpd_req_t* request)
        {
            return SendFixed(request,
                             VIEWER_ASSET_CONTRACT::kHtmlMime,
                             nullptr,
                             VIEWER_ASSET_CONTRACT::kNoStoreCacheControl,
                             g_webPages->viewer_index_html,
                             VIEWER_ASSET_CONTRACT::kIndexBytes);
        }

        esp_err_t ManifestHandler(httpd_req_t* request)
        {
            return SendFixed(request,
                             VIEWER_ASSET_CONTRACT::kJsonMime,
                             nullptr,
                             VIEWER_ASSET_CONTRACT::kNoStoreCacheControl,
                             g_webPages->viewer_asset_manifest,
                             VIEWER_ASSET_CONTRACT::kManifestBytes);
        }

        esp_err_t CssHandler(httpd_req_t* request)
        {
            return SendFixed(request,
                             VIEWER_ASSET_CONTRACT::kCssMime,
                             VIEWER_ASSET_CONTRACT::kGzipEncoding,
                             VIEWER_ASSET_CONTRACT::kImmutableCacheControl,
                             g_webPages->viewer_css_gzip,
                             VIEWER_ASSET_CONTRACT::kCssGzipBytes);
        }

        esp_err_t JsHandler(httpd_req_t* request)
        {
            return SendFixed(request,
                             VIEWER_ASSET_CONTRACT::kJsMime,
                             VIEWER_ASSET_CONTRACT::kGzipEncoding,
                             VIEWER_ASSET_CONTRACT::kImmutableCacheControl,
                             g_webPages->viewer_js_gzip,
                             VIEWER_ASSET_CONTRACT::kJsGzipBytes);
        }

        esp_err_t DeviceHandler(httpd_req_t* request)
        {
            return SendFixed(request,
                             VIEWER_ASSET_CONTRACT::kJsonMime,
                             nullptr,
                             VIEWER_ASSET_CONTRACT::kNoStoreCacheControl,
                             reinterpret_cast<const std::uint8_t*>(VIEWER_ASSET_CONTRACT::kDeviceJson),
                             VIEWER_ASSET_CONTRACT::kDeviceJsonBytes);
        }

        httpd_uri_t MakeUri(const char* uri, esp_err_t (*handler)(httpd_req_t*))
        {
            httpd_uri_t descriptor = {};
            descriptor.uri = uri;
            descriptor.method = HTTP_GET;
            descriptor.handler = handler;
            descriptor.user_ctx = nullptr;
            descriptor.is_websocket = false;
            descriptor.handle_ws_control_frames = false;
            descriptor.supported_subprotocol = nullptr;
            return descriptor;
        }

        void InitializeDescriptors()
        {
            g_routes[0] = MakeUri(VIEWER_ASSET_CONTRACT::kRootRoute, RootHandler);
            g_routes[1] = MakeUri(VIEWER_ASSET_CONTRACT::kViewerRoute, IndexHandler);
            g_routes[2] = MakeUri(VIEWER_ASSET_CONTRACT::kManifestRoute, ManifestHandler);
            g_routes[3] = MakeUri(VIEWER_ASSET_CONTRACT::kCssRoute, CssHandler);
            g_routes[4] = MakeUri(VIEWER_ASSET_CONTRACT::kJsRoute, JsHandler);
            g_routes[5] = MakeUri(VIEWER_ASSET_CONTRACT::kDeviceRoute, DeviceHandler);
        }
    } // namespace

    bool HasExpectedAssetIdentity(const WebPagePool_t* webPages)
    {
        return webPages != nullptr &&
               VIEWER_ASSET_CONTRACT::IsExpectedBundleId(webPages->viewer_bundle_id, sizeof(webPages->viewer_bundle_id));
    }

    bool Register(httpd_handle_t server, const WebPagePool_t* webPages)
    {
        if (server == nullptr || !HasExpectedAssetIdentity(webPages))
        {
            ESP_LOGE(kTag, "VIEWER_ASSETPOOL_IDENTITY_MISMATCH");
            return false;
        }

        g_webPages = webPages;
        InitializeDescriptors();
        std::size_t registered = 0;
        for (; registered < VIEWER_ASSET_CONTRACT::kViewerRouteCount; ++registered)
        {
            const esp_err_t result = httpd_register_uri_handler(server, &g_routes[registered]);
            if (result != ESP_OK)
            {
                ESP_LOGE(kTag, "Viewer route registration failed: %s", esp_err_to_name(result));
                while (registered != 0U)
                {
                    --registered;
                    httpd_unregister_uri_handler(server, g_routes[registered].uri, g_routes[registered].method);
                }
                g_webPages = nullptr;
                return false;
            }
        }
        return true;
    }
} // namespace VIEWER_HTTP_ROUTES
