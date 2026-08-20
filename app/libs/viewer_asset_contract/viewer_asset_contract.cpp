#include "viewer_asset_contract.h"

#include <cstring>

namespace VIEWER_ASSET_CONTRACT
{
    const char kViewerBundleId[] = "042b48e37a7f18b71f1ba89b2188391a87413c7e38d79ef0b380822ce7e0b894";
    const char kIndexSha256[] = "1d3e9e7b09d47a1b52c2f584d6f95dae94944c47226ebf76da82b5b367aebbf7";
    const char kManifestSha256[] = "042b48e37a7f18b71f1ba89b2188391a87413c7e38d79ef0b380822ce7e0b894";
    const char kCssGzipSha256[] = "5e7442c9aa36fbcb6f5b97b3ddedebc7792d0bc136dbcb0aa1a5f1e5af2cf7e9";
    const char kJsGzipSha256[] = "b19cd742a1d7085934f9b89745d191e11a2c0b5a92798b8db292f37aaa357166";

    const char kRootRoute[] = "/";
    const char kViewerRoute[] = "/viewer/";
    const char kManifestRoute[] = "/viewer/asset-manifest.json";
    const char kCssRoute[] = "/viewer/assets/app.5e7442c9aa36fbcb6f5b97b3ddedebc7792d0bc136dbcb0aa1a5f1e5af2cf7e9.css";
    const char kJsRoute[] = "/viewer/assets/app.b19cd742a1d7085934f9b89745d191e11a2c0b5a92798b8db292f37aaa357166.js";
    const char kDeviceRoute[] = "/viewer/device.json";

    const char kHtmlMime[] = "text/html; charset=utf-8";
    const char kJsonMime[] = "application/json; charset=utf-8";
    const char kCssMime[] = "text/css; charset=utf-8";
    const char kJsMime[] = "application/javascript; charset=utf-8";
    const char kIdentityEncoding[] = "identity";
    const char kGzipEncoding[] = "gzip";
    const char kNoStoreCacheControl[] = "no-store";
    const char kImmutableCacheControl[] = "public, max-age=31536000, immutable";

    const char kIndexEnvironmentVariable[] = "VAMETER_VIEWER_INDEX_PATH";
    const char kManifestEnvironmentVariable[] = "VAMETER_VIEWER_MANIFEST_PATH";
    const char kCssGzipEnvironmentVariable[] = "VAMETER_VIEWER_CSS_GZIP_PATH";
    const char kJsGzipEnvironmentVariable[] = "VAMETER_VIEWER_JS_GZIP_PATH";

    const char kDeviceSchemaVersionField[] = "schema_version";
    const char kDeviceViewerBundleIdField[] = "viewer_bundle_id";
    const char kDeviceD2bProtocolField[] = "d2b_protocol";
    const char kDeviceD2bStreamField[] = "d2b_stream";
    const char kD2bProtocolValue[] = "d2b-stream/0.1";
    const char kD2bStreamValue[] = "live-vi";
    const char kDeviceJson[] =
        "{\"schema_version\":1,\"viewer_bundle_id\":\"042b48e37a7f18b71f1ba89b2188391a87413c7e38d79ef0b380822ce7e0b894\","
        "\"d2b_protocol\":\"d2b-stream/0.1\",\"d2b_stream\":\"live-vi\"}";
    const std::size_t kDeviceJsonBytes = sizeof(kDeviceJson) - 1U;

    namespace
    {
        const RouteContract kRoutes[kViewerRouteCount] = {
            {"GET", kRootRoute, "", kIdentityEncoding, kNoStoreCacheControl},
            {"GET", kViewerRoute, kHtmlMime, kIdentityEncoding, kNoStoreCacheControl},
            {"GET", kManifestRoute, kJsonMime, kIdentityEncoding, kNoStoreCacheControl},
            {"GET", kCssRoute, kCssMime, kGzipEncoding, kImmutableCacheControl},
            {"GET", kJsRoute, kJsMime, kGzipEncoding, kImmutableCacheControl},
            {"GET", kDeviceRoute, kJsonMime, kIdentityEncoding, kNoStoreCacheControl},
        };
    } // namespace

    const RouteContract* ViewerRoutes() { return kRoutes; }

    bool IsExpectedBundleId(const std::uint8_t* bytes, std::size_t capacity)
    {
        return bytes != nullptr && capacity == kBundleIdCapacity && bytes[kBundleIdCharacters] == 0U &&
               std::memcmp(bytes, kViewerBundleId, kBundleIdCharacters) == 0;
    }
} // namespace VIEWER_ASSET_CONTRACT
