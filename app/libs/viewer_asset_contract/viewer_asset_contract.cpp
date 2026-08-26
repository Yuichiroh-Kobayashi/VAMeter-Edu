#include "viewer_asset_contract.h"

#include <cstdio>
#include <cstring>

namespace VIEWER_ASSET_CONTRACT
{
    const char kViewerBundleId[] = "fbe7f2a9033e8f957d0460ec8ff929298e2073de3e292037846237cab6422701";
    const char kIndexSha256[] = "d19182296e250e4eb5443eabcdb6a9ad1cf67ffae90b51bb25f27995579d6039";
    const char kManifestSha256[] = "fbe7f2a9033e8f957d0460ec8ff929298e2073de3e292037846237cab6422701";
    const char kCssGzipSha256[] = "1fbcc3ae1fca202d5e3e4858cc74d5a9d4b07d18f921c7fe0981e1c08fa9a741";
    const char kJsGzipSha256[] = "aac86498c7bb562a9a111bb501be5f4585336f0c153ce809007c2a2b7efe505d";

    const char kRootRoute[] = "/";
    const char kViewerRoute[] = "/viewer/";
    const char kManifestRoute[] = "/viewer/asset-manifest.json";
    const char kCssRoute[] = "/viewer/assets/app.1fbcc3ae1fca202d5e3e4858cc74d5a9d4b07d18f921c7fe0981e1c08fa9a741.css";
    const char kJsRoute[] = "/viewer/assets/app.aac86498c7bb562a9a111bb501be5f4585336f0c153ce809007c2a2b7efe505d.js";
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
