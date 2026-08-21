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
    Expect(kJsGzipBytes == 18790U, "JS gzip capacity");
    Expect(kIndexBytes + kManifestBytes + kCssGzipBytes + kJsGzipBytes == kStoredPayloadBytes, "stored payload arithmetic");
    Expect(kStoredPayloadBytes == 21306U, "stored payload bytes");
    Expect(std::strlen(kViewerBundleId) == kBundleIdCharacters, "bundle ID length");
    Expect(kBundleIdCapacity == 65U, "bundle ID storage capacity");

    Expect(Equal(kIndexSha256, "1d3e9e7b09d47a1b52c2f584d6f95dae94944c47226ebf76da82b5b367aebbf7"), "index SHA-256");
    Expect(Equal(kManifestSha256, kViewerBundleId), "manifest SHA-256 and bundle ID");
    Expect(Equal(kCssGzipSha256, "5e7442c9aa36fbcb6f5b97b3ddedebc7792d0bc136dbcb0aa1a5f1e5af2cf7e9"), "CSS SHA-256");
    Expect(Equal(kJsGzipSha256, "b19cd742a1d7085934f9b89745d191e11a2c0b5a92798b8db292f37aaa357166"), "JS SHA-256");

    const char* const expectedRoutes[kViewerRouteCount] = {
        "/",
        "/viewer/",
        "/viewer/asset-manifest.json",
        "/viewer/assets/app.5e7442c9aa36fbcb6f5b97b3ddedebc7792d0bc136dbcb0aa1a5f1e5af2cf7e9.css",
        "/viewer/assets/app.b19cd742a1d7085934f9b89745d191e11a2c0b5a92798b8db292f37aaa357166.js",
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

    Expect(DisplayProfileForWaveformModeCode(1U) == DisplayProfile::Voltage,
           "mode_volt_only maps to Voltage");
    Expect(DisplayProfileForWaveformModeCode(2U) == DisplayProfile::Current,
           "mode_current_only maps to Current");
    Expect(DisplayProfileForWaveformModeCode(0U) == DisplayProfile::Both, "mode_both maps to Both");
    Expect(DisplayProfileForWaveformModeCode(3U) == DisplayProfile::Invalid,
           "unknown waveform mode does not fall back to Both");

    const DisplayProfile profiles[] = {DisplayProfile::Voltage, DisplayProfile::Current, DisplayProfile::Both};
    const char* const displayNames[] = {"Voltage", "Current", "Both"};
    for (std::size_t index = 0; index < 3U; ++index)
    {
        char deviceJson[kDeviceJsonCapacity] = {};
        std::size_t deviceJsonBytes = 0U;
        Expect(BuildDeviceJson(profiles[index], deviceJson, sizeof(deviceJson), &deviceJsonBytes),
               "device JSON builds for exact display profile");
        Expect(deviceJsonBytes == std::strlen(deviceJson), "device JSON explicit byte length");
        Expect(std::strstr(deviceJson, "\"schema_version\":1") != nullptr, "device schema version retained");
        Expect(std::strstr(deviceJson, kViewerBundleId) != nullptr, "device bundle ID retained");
        Expect(std::strstr(deviceJson, "\"d2b_protocol\":\"d2b-stream/0.1\"") != nullptr,
               "device D2B protocol retained");
        Expect(std::strstr(deviceJson, "\"d2b_stream\":\"live-vi\"") != nullptr,
               "device D2B stream retained");
        const std::string displayField = std::string("\"display_name\":\"") + displayNames[index] + "\"";
        Expect(std::strstr(deviceJson, displayField.c_str()) != nullptr, "device exact display_name");
        Expect(std::strstr(deviceJson, "mac") == nullptr, "device JSON has no MAC field");
        Expect(std::strstr(deviceJson, "serial") == nullptr, "device JSON has no serial field");
        Expect(std::strstr(deviceJson, "ssid") == nullptr, "device JSON has no SSID field");
    }
    char invalidJson[kDeviceJsonCapacity] = {};
    std::size_t invalidJsonBytes = 0U;
    Expect(!BuildDeviceJson(DisplayProfile::Invalid,
                            invalidJson,
                            sizeof(invalidJson),
                            &invalidJsonBytes),
           "invalid display profile fails closed");

    DisplayProfileSession session;
    Expect(session.begin(DisplayProfile::Voltage), "display profile starts once");
    Expect(!session.begin(DisplayProfile::Current), "active display profile is immutable");
    Expect(session.profile() == DisplayProfile::Voltage, "active profile remains unchanged");
    session.end();
    Expect(session.begin(DisplayProfile::Current), "new lifecycle can select a new profile");
    session.end();
    Expect(!session.begin(DisplayProfile::Invalid), "invalid profile cannot start a session");

    std::uint8_t bundleId[kBundleIdCapacity] = {};
    std::memcpy(bundleId, kViewerBundleId, kBundleIdCapacity);
    Expect(IsExpectedBundleId(bundleId, sizeof(bundleId)), "exact bundle ID accepted");
    bundleId[kBundleIdCharacters] = 'x';
    Expect(!IsExpectedBundleId(bundleId, sizeof(bundleId)), "unterminated bundle ID rejected");
    bundleId[kBundleIdCharacters] = 0U;
    bundleId[0] = (bundleId[0] == static_cast<std::uint8_t>('0')) ? static_cast<std::uint8_t>('1') : static_cast<std::uint8_t>('0');
    Expect(!IsExpectedBundleId(bundleId, sizeof(bundleId)), "mismatched bundle ID rejected");

    std::cout << "PASS: frozen Viewer product contract\n";
    return 0;
}
