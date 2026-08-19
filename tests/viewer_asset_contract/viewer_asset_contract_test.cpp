#include "viewer_asset_contract.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    bool Equal(const char* left, const char* right) { return std::strcmp(left, right) == 0; }
} // namespace

int main()
{
    using namespace VIEWER_ASSET_CONTRACT;

    Expect(kViewerRouteCount == 6U, "Viewer route count");
    Expect(kD2bRouteCount == 4U, "D2B route count");
    Expect(kSystemLiveRouteCount == 10U, "SystemLive route count");
    Expect(kViewerRouteCount + kD2bRouteCount == kSystemLiveRouteCount, "SystemLive route arithmetic");

    Expect(kIndexBytes == 573U, "index capacity");
    Expect(kManifestBytes == 1363U, "manifest capacity");
    Expect(kCssGzipBytes == 580U, "CSS gzip capacity");
    Expect(kJsGzipBytes == 18785U, "JS gzip capacity");
    Expect(kIndexBytes + kManifestBytes + kCssGzipBytes + kJsGzipBytes == kStoredPayloadBytes, "stored payload arithmetic");
    Expect(kStoredPayloadBytes == 21301U, "stored payload bytes");
    Expect(std::strlen(kViewerBundleId) == kBundleIdCharacters, "bundle ID length");
    Expect(kBundleIdCapacity == 65U, "bundle ID storage capacity");

    Expect(Equal(kIndexSha256, "b06f9c7e7f5aa788ba5743bd27700fa3594a1907af8521e5e3acc920f1e23ce7"), "index SHA-256");
    Expect(Equal(kManifestSha256, kViewerBundleId), "manifest SHA-256 and bundle ID");
    Expect(Equal(kCssGzipSha256, "5e7442c9aa36fbcb6f5b97b3ddedebc7792d0bc136dbcb0aa1a5f1e5af2cf7e9"), "CSS SHA-256");
    Expect(Equal(kJsGzipSha256, "bd9a5a158ab1f58d7c0f147e933d1a35911c6e64f47e676c84e3aa5d172a7e54"), "JS SHA-256");

    const char* const expectedRoutes[kViewerRouteCount] = {
        "/",
        "/viewer/",
        "/viewer/asset-manifest.json",
        "/viewer/assets/app.5e7442c9aa36fbcb6f5b97b3ddedebc7792d0bc136dbcb0aa1a5f1e5af2cf7e9.css",
        "/viewer/assets/app.bd9a5a158ab1f58d7c0f147e933d1a35911c6e64f47e676c84e3aa5d172a7e54.js",
        "/viewer/device.json",
    };
    const RouteContract* routes = ViewerRoutes();
    for (std::size_t index = 0; index < kViewerRouteCount; ++index)
    {
        Expect(Equal(routes[index].method, "GET"), "Viewer methods are read-only GET");
        Expect(Equal(routes[index].uri, expectedRoutes[index]), "exact Viewer route");
        const std::string uri(routes[index].uri);
        Expect(uri.find("config") == std::string::npos, "no configuration route");
        Expect(uri.find("write") == std::string::npos, "no write route");
    }

    Expect(Equal(routes[0].cacheControl, kNoStoreCacheControl), "root no-store");
    Expect(Equal(routes[1].cacheControl, kNoStoreCacheControl), "Viewer index no-store");
    Expect(Equal(routes[2].cacheControl, kNoStoreCacheControl), "manifest no-store");
    Expect(Equal(routes[5].cacheControl, kNoStoreCacheControl), "device JSON no-store");
    Expect(Equal(routes[3].cacheControl, kImmutableCacheControl), "hashed CSS immutable");
    Expect(Equal(routes[4].cacheControl, kImmutableCacheControl), "hashed JS immutable");
    Expect(Equal(routes[3].mime, "text/css; charset=utf-8"), "CSS MIME");
    Expect(Equal(routes[4].mime, "application/javascript; charset=utf-8"), "JS MIME");
    Expect(Equal(routes[3].contentEncoding, kGzipEncoding), "CSS gzip Content-Encoding");
    Expect(Equal(routes[4].contentEncoding, kGzipEncoding), "JS gzip Content-Encoding");

    Expect(std::strstr(kDeviceJson, "\"schema_version\":1") != nullptr, "device schema version");
    Expect(std::strstr(kDeviceJson, kViewerBundleId) != nullptr, "device bundle ID");
    Expect(std::strstr(kDeviceJson, "\"d2b_protocol\":\"d2b-stream/0.1\"") != nullptr, "device D2B protocol");
    Expect(std::strstr(kDeviceJson, "\"d2b_stream\":\"live-vi\"") != nullptr, "device D2B stream");
    Expect(kDeviceJsonBytes == std::strlen(kDeviceJson), "device JSON explicit byte length");

    std::uint8_t bundleId[kBundleIdCapacity] = {};
    std::memcpy(bundleId, kViewerBundleId, kBundleIdCapacity);
    Expect(IsExpectedBundleId(bundleId, sizeof(bundleId)), "exact bundle ID accepted");
    bundleId[kBundleIdCharacters] = 'x';
    Expect(!IsExpectedBundleId(bundleId, sizeof(bundleId)), "unterminated bundle ID rejected");
    bundleId[kBundleIdCharacters] = 0U;
    bundleId[0] = '0';
    Expect(!IsExpectedBundleId(bundleId, sizeof(bundleId)), "mismatched bundle ID rejected");

    std::cout << "PASS: frozen Viewer product contract\n";
    return 0;
}
