#include "viewer_asset_contract.h"

#include <cstdio>
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
    const char kDeviceDisplayNameField[] = "display_name";
    const char kD2bProtocolValue[] = "d2b-stream/0.1";
    const char kD2bStreamValue[] = "live-vi";

    DisplayProfile DisplayProfileForWaveformModeCode(std::uint8_t modeCode)
    {
        switch (modeCode)
        {
        case 0U:
            return DisplayProfile::Both;
        case 1U:
            return DisplayProfile::Voltage;
        case 2U:
            return DisplayProfile::Current;
        default:
            return DisplayProfile::Invalid;
        }
    }

    const char* DisplayName(DisplayProfile profile)
    {
        switch (profile)
        {
        case DisplayProfile::Voltage:
            return "Voltage";
        case DisplayProfile::Current:
            return "Current";
        case DisplayProfile::Both:
            return "Both";
        case DisplayProfile::Invalid:
        default:
            return nullptr;
        }
    }

    bool BuildDeviceJson(DisplayProfile profile, char* output, std::size_t capacity, std::size_t* bytesWritten)
    {
        const char* const displayName = DisplayName(profile);
        if (displayName == nullptr || output == nullptr || capacity == 0U || bytesWritten == nullptr)
            return false;

        const int result = std::snprintf(output,
                                         capacity,
                                         "{\"schema_version\":1,\"viewer_bundle_id\":\"%s\","
                                         "\"d2b_protocol\":\"%s\",\"d2b_stream\":\"%s\","
                                         "\"display_name\":\"%s\"}",
                                         kViewerBundleId,
                                         kD2bProtocolValue,
                                         kD2bStreamValue,
                                         displayName);
        if (result < 0 || static_cast<std::size_t>(result) >= capacity)
            return false;

        *bytesWritten = static_cast<std::size_t>(result);
        return true;
    }

    DisplayProfileSession::DisplayProfileSession() : _profile(DisplayProfile::Invalid), _active(false) {}

    bool DisplayProfileSession::begin(DisplayProfile profile)
    {
        if (_active || DisplayName(profile) == nullptr)
            return false;
        _profile = profile;
        _active = true;
        return true;
    }

    void DisplayProfileSession::end()
    {
        _profile = DisplayProfile::Invalid;
        _active = false;
    }

    bool DisplayProfileSession::active() const { return _active; }

    DisplayProfile DisplayProfileSession::profile() const { return _profile; }

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
