#pragma once

#include <cstddef>
#include <cstdint>

namespace VIEWER_ASSET_CONTRACT
{
    static const std::size_t kIndexBytes = 573U;
    static const std::size_t kManifestBytes = 1363U;
    static const std::size_t kCssGzipBytes = 830U;
    static const std::size_t kJsGzipBytes = 24874U;
    static const std::size_t kStoredPayloadBytes = 27640U;
    static const std::size_t kBundleIdCharacters = 64U;
    static const std::size_t kBundleIdCapacity = 65U;
    static const std::size_t kViewerRouteCount = 6U;
    static const std::size_t kD2bRouteCount = 4U;
    static const std::size_t kSystemLiveRouteCount = 10U;

    extern const char kViewerBundleId[];
    extern const char kIndexSha256[];
    extern const char kManifestSha256[];
    extern const char kCssGzipSha256[];
    extern const char kJsGzipSha256[];

    extern const char kRootRoute[];
    extern const char kViewerRoute[];
    extern const char kManifestRoute[];
    extern const char kCssRoute[];
    extern const char kJsRoute[];
    extern const char kDeviceRoute[];

    extern const char kHtmlMime[];
    extern const char kJsonMime[];
    extern const char kCssMime[];
    extern const char kJsMime[];
    extern const char kIdentityEncoding[];
    extern const char kGzipEncoding[];
    extern const char kNoStoreCacheControl[];
    extern const char kImmutableCacheControl[];

    extern const char kIndexEnvironmentVariable[];
    extern const char kManifestEnvironmentVariable[];
    extern const char kCssGzipEnvironmentVariable[];
    extern const char kJsGzipEnvironmentVariable[];

    extern const char kDeviceSchemaVersionField[];
    extern const char kDeviceViewerBundleIdField[];
    extern const char kDeviceD2bProtocolField[];
    extern const char kDeviceD2bStreamField[];
    extern const char kDeviceDisplayNameField[];
    extern const char kD2bProtocolValue[];
    extern const char kD2bStreamValue[];
    static const std::size_t kDeviceJsonCapacity = 256U;

    enum class DisplayProfile : std::uint8_t
    {
        Invalid = 0,
        Voltage,
        Current,
        Both,
    };

    DisplayProfile DisplayProfileForWaveformModeCode(std::uint8_t modeCode);
    const char* DisplayName(DisplayProfile profile);
    bool BuildDeviceJson(DisplayProfile profile, char* output, std::size_t capacity, std::size_t* bytesWritten);

    class DisplayProfileSession
    {
    public:
        DisplayProfileSession();

        bool begin(DisplayProfile profile);
        void end();
        bool active() const;
        DisplayProfile profile() const;

    private:
        DisplayProfile _profile;
        bool _active;
    };

    struct RouteContract
    {
        const char* method;
        const char* uri;
        const char* mime;
        const char* contentEncoding;
        const char* cacheControl;
    };

    const RouteContract* ViewerRoutes();
    bool IsExpectedBundleId(const std::uint8_t* bytes, std::size_t capacity);
} // namespace VIEWER_ASSET_CONTRACT
