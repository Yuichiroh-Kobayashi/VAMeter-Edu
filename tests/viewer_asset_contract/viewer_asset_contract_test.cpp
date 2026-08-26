#include "viewer_asset_contract.h"
#include "viewer_http_routes.h"
#include "assets/web/types.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
    std::size_t gRegisteredRouteCount = 0U;

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

extern "C" esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t*)
{
    ++gRegisteredRouteCount;
    return ESP_OK;
}

extern "C" esp_err_t httpd_unregister_uri_handler(httpd_handle_t, const char*, httpd_method_t) { return ESP_OK; }
extern "C" esp_err_t httpd_resp_set_hdr(httpd_req_t*, const char*, const char*) { return ESP_OK; }
extern "C" esp_err_t httpd_resp_set_status(httpd_req_t*, const char*) { return ESP_OK; }
extern "C" esp_err_t httpd_resp_set_type(httpd_req_t*, const char*) { return ESP_OK; }
extern "C" esp_err_t httpd_resp_send(httpd_req_t*, const char*, std::size_t) { return ESP_OK; }
extern "C" esp_err_t httpd_resp_send_err(httpd_req_t*, httpd_err_code_t, const char*) { return ESP_OK; }
extern "C" const char* esp_err_to_name(esp_err_t) { return "test error"; }

int main()
{
    using namespace VIEWER_ASSET_CONTRACT;

    Expect(kViewerRouteCount == 6U, "Viewer route count");
    Expect(kD2bRouteCount == 4U, "D2B route count");
    Expect(kSystemLiveRouteCount == 10U, "SystemLive route count");
    Expect(kViewerRouteCount + kD2bRouteCount == kSystemLiveRouteCount, "SystemLive route arithmetic");

    Expect(kIndexBytes == 573U, "index capacity");
    Expect(kManifestBytes == 1363U, "manifest capacity");
    Expect(kCssGzipBytes == 830U, "CSS gzip payload bytes");
    Expect(kJsGzipBytes == 24958U, "JS gzip payload bytes");
    Expect(kIndexBytes + kManifestBytes + kCssGzipBytes + kJsGzipBytes == kStoredPayloadBytes, "stored payload arithmetic");
    Expect(kStoredPayloadBytes == 27724U, "stored payload bytes");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_index_html) == kIndexBytes, "index physical slot");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_asset_manifest) == kManifestBytes, "manifest physical slot");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_css_gzip) == kCssGzipBytes, "CSS physical slot");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_js_gzip) == kJsGzipBytes, "JS physical slot");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_bundle_id) == kBundleIdCapacity, "bundle ID physical slot");
    Expect(std::strlen(kViewerBundleId) == kBundleIdCharacters, "bundle ID length");
    Expect(kBundleIdCapacity == 65U, "bundle ID storage capacity");

    Expect(kViewerBundleId[kBundleIdCharacters] == '\0', "bundle ID NUL terminator");
    Expect(Equal(kIndexSha256, "d19182296e250e4eb5443eabcdb6a9ad1cf67ffae90b51bb25f27995579d6039"), "index SHA-256");
    Expect(Equal(kManifestSha256, kViewerBundleId), "manifest SHA-256 and bundle ID");
    Expect(Equal(kCssGzipSha256, "1fbcc3ae1fca202d5e3e4858cc74d5a9d4b07d18f921c7fe0981e1c08fa9a741"), "CSS SHA-256");
    Expect(Equal(kJsGzipSha256, "aac86498c7bb562a9a111bb501be5f4585336f0c153ce809007c2a2b7efe505d"), "JS SHA-256");
    Expect(Equal(kViewerBundleId, "fbe7f2a9033e8f957d0460ec8ff929298e2073de3e292037846237cab6422701"),
           "frozen fbe7 candidate bundle ID");

    const char* const expectedRoutes[kViewerRouteCount] = {
        "/",
        "/viewer/",
        "/viewer/asset-manifest.json",
        "/viewer/assets/app.1fbcc3ae1fca202d5e3e4858cc74d5a9d4b07d18f921c7fe0981e1c08fa9a741.css",
        "/viewer/assets/app.aac86498c7bb562a9a111bb501be5f4585336f0c153ce809007c2a2b7efe505d.js",
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

    Expect(DisplayProfileForWaveformModeCode(1U) == DisplayProfile::Voltage, "mode_volt_only maps to Voltage");
    Expect(DisplayProfileForWaveformModeCode(2U) == DisplayProfile::Current, "mode_current_only maps to Current");
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
        Expect(std::strstr(deviceJson, "\"d2b_protocol\":\"d2b-stream/0.1\"") != nullptr, "device D2B protocol retained");
        Expect(std::strstr(deviceJson, "\"d2b_stream\":\"live-vi\"") != nullptr, "device D2B stream retained");
        const std::string displayField = std::string("\"display_name\":\"") + displayNames[index] + "\"";
        Expect(std::strstr(deviceJson, displayField.c_str()) != nullptr, "device exact display_name");
        Expect(std::strstr(deviceJson, "mac") == nullptr, "device JSON has no MAC field");
        Expect(std::strstr(deviceJson, "serial") == nullptr, "device JSON has no serial field");
        Expect(std::strstr(deviceJson, "ssid") == nullptr, "device JSON has no SSID field");
    }
    char invalidJson[kDeviceJsonCapacity] = {};
    std::size_t invalidJsonBytes = 0U;
    Expect(!BuildDeviceJson(DisplayProfile::Invalid, invalidJson, sizeof(invalidJson), &invalidJsonBytes),
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
    bundleId[0] =
        (bundleId[0] == static_cast<std::uint8_t>('0')) ? static_cast<std::uint8_t>('1') : static_cast<std::uint8_t>('0');
    Expect(!IsExpectedBundleId(bundleId, sizeof(bundleId)), "mismatched bundle ID rejected");

    WebPagePool_t mismatchedAssetPool = {};
    const char staleBundleId[] = "4789b3bf99e923a859a38f6accbdc23a41e9f718099b4b56a7f7d2f13470009d";
    static_assert(sizeof(staleBundleId) == kBundleIdCapacity, "stale bundle ID storage size");
    std::memcpy(mismatchedAssetPool.viewer_bundle_id, staleBundleId, sizeof(staleBundleId));
    Expect(!IsExpectedBundleId(mismatchedAssetPool.viewer_bundle_id, sizeof(mismatchedAssetPool.viewer_bundle_id)),
           "stale 4789 bundle ID rejected");
    gRegisteredRouteCount = 0U;
    Expect(!VIEWER_HTTP_ROUTES::Register(reinterpret_cast<httpd_handle_t>(1), &mismatchedAssetPool, DisplayProfile::Voltage),
           "mismatched AssetPool registration fails closed");
    Expect(gRegisteredRouteCount == 0U, "mismatched AssetPool registers no Viewer routes");

    std::cout << "PASS: frozen Viewer product contract\n";
    return 0;
}
