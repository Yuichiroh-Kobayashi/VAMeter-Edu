#include "viewer_asset_contract.h"
#include "viewer_http_routes.h"
#include "assets/web/types.h"
#include "libs/asset_pool_layout/asset_pool_layout.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
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

    // Test-only file seam: use the production Tier 1 validator, then the real Tier 2
    // identity/registration code with HTTP shims. No AssetPool access precedes Tier 1.
    int InspectContainer(const char* path)
    {
        using namespace ASSET_POOL_LAYOUT;
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() != static_cast<std::streamoff>(kAssetPoolPartitionBytes))
            return 3;
        std::vector<std::uint8_t> data(kAssetPoolPartitionBytes);
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(data.data()), data.size()))
            return 3;
        const Result layout = ValidateContainer(data.data(), data.size());
        std::cout << "LAYOUT_RESULT=" << ResultName(layout) << '\n';
        if (layout != Result::Ok)
            return 1;
        std::unique_ptr<WebPagePool_t> webPages(new WebPagePool_t);
        std::memcpy(webPages.get(), data.data() + kWebPageOffset, sizeof(WebPagePool_t));
        const bool identity = VIEWER_HTTP_ROUTES::HasExpectedAssetIdentity(webPages.get());
        gRegisteredRouteCount = 0;
        const bool registered = VIEWER_HTTP_ROUTES::Register(
            reinterpret_cast<httpd_handle_t>(1), webPages.get(), VIEWER_ASSET_CONTRACT::DisplayProfile::Voltage);
        Expect(registered == identity, "registration follows exact Viewer identity");
        Expect(gRegisteredRouteCount == (identity ? VIEWER_ASSET_CONTRACT::kViewerRouteCount : 0U),
               "all Viewer routes or zero routes according to identity");
        VIEWER_HTTP_ROUTES::Reset();
        std::cout << "VIEWER_IDENTITY=" << (identity ? "PASS" : "FAIL") << '\n';
        return identity ? 0 : 2;
    }
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

int main(int argc, char** argv)
{
    if (argc == 3 && std::strcmp(argv[1], "--container") == 0)
        return InspectContainer(argv[2]);
    if (argc != 1)
        return 3;
    using namespace VIEWER_ASSET_CONTRACT;

    Expect(kViewerRouteCount == 6U, "Viewer route count");
    Expect(kD2bRouteCount == 4U, "D2B route count");
    Expect(kSystemLiveRouteCount == 10U, "SystemLive route count");
    Expect(kViewerRouteCount + kD2bRouteCount == kSystemLiveRouteCount, "SystemLive route arithmetic");

    Expect(kIndexBytes == 573U, "index capacity");
    Expect(kManifestBytes == 1364U, "manifest capacity");
    Expect(kCssGzipBytes == 2669U, "CSS gzip payload bytes");
    Expect(kJsGzipBytes == 30168U, "JS gzip payload bytes");
    Expect(kIndexBytes + kManifestBytes + kCssGzipBytes + kJsGzipBytes == kStoredPayloadBytes, "stored payload arithmetic");
    Expect(kStoredPayloadBytes == 34774U, "stored payload bytes");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_index_html) == kIndexBytes, "index physical slot");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_asset_manifest) == kManifestBytes, "manifest physical slot");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_css_gzip) == kCssGzipBytes, "CSS physical slot");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_js_gzip) == kJsGzipBytes, "JS physical slot");
    Expect(sizeof(((WebPagePool_t*)nullptr)->viewer_bundle_id) == kBundleIdCapacity, "bundle ID physical slot");
    Expect(std::strlen(kViewerBundleId) == kBundleIdCharacters, "bundle ID length");
    Expect(kBundleIdCapacity == 65U, "bundle ID storage capacity");

    Expect(kViewerBundleId[kBundleIdCharacters] == '\0', "bundle ID NUL terminator");
    Expect(Equal(kIndexSha256, "2275800d59506344ed693c914fbebe09b701351cdc1c568ef61ce752c1f74781"), "index SHA-256");
    Expect(Equal(kManifestSha256, kViewerBundleId), "manifest SHA-256 and bundle ID");
    Expect(Equal(kCssGzipSha256, "ad1eafe9be7c08ae40198db88c0e892822a0497731644138bfc2e93515f8f015"), "CSS SHA-256");
    Expect(Equal(kJsGzipSha256, "2c7925c88541d26de3871fcc7362f7f9002164886a6e4666779a5e330fd69253"), "JS SHA-256");
    Expect(Equal(kViewerBundleId, "01e39e5c3230bc2c3a277659014031f6f955864e5a0886c15fa004114b89c973"),
           "final post-v2 Viewer intake bundle ID");

    const char* const expectedRoutes[kViewerRouteCount] = {
        "/",
        "/viewer/",
        "/viewer/asset-manifest.json",
        "/viewer/assets/app.ad1eafe9be7c08ae40198db88c0e892822a0497731644138bfc2e93515f8f015.css",
        "/viewer/assets/app.2c7925c88541d26de3871fcc7362f7f9002164886a6e4666779a5e330fd69253.js",
        "/viewer/device.json",
    };
    const RouteContract* routes = ViewerRoutes();
    for (std::size_t index = 0; index < kViewerRouteCount; ++index)
    {
        Expect(Equal(routes[index].method, "GET"), "Viewer methods are read-only GET");
        Expect(Equal(routes[index].uri, expectedRoutes[index]), "exact Viewer route");
        Expect(!Equal(routes[index].uri,
                      "/viewer/assets/app.250905db503bf774bcab87f29e44ceddd63c949e798cc72c55864b933a8cfafb.css"),
               "stable CSS route absent");
        Expect(
            !Equal(routes[index].uri, "/viewer/assets/app.46bafb3d23345cd8dc48533c4c59c595ef6b0ae5cf051158dae509a20f56cbe4.js"),
            "stable JS route absent");
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

    const std::uint8_t shaTestVector[] = {'a', 'b', 'c'};
    const char shaTestVectorDigest[] = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    Expect(MatchesSha256(shaTestVector, sizeof(shaTestVector), sizeof(shaTestVector), shaTestVectorDigest),
           "known SHA-256 vector accepted");
    Expect(!MatchesSha256(nullptr, sizeof(shaTestVector), sizeof(shaTestVector), shaTestVectorDigest),
           "missing representation rejected");
    Expect(!MatchesSha256(shaTestVector, sizeof(shaTestVector) - 1U, sizeof(shaTestVector), shaTestVectorDigest),
           "one-byte-short representation rejected");
    std::uint8_t mutatedShaTestVector[sizeof(shaTestVector)] = {'a', 'b', 'c'};
    mutatedShaTestVector[1] ^= 0x01U;
    Expect(
        !MatchesSha256(mutatedShaTestVector, sizeof(mutatedShaTestVector), sizeof(mutatedShaTestVector), shaTestVectorDigest),
        "one-byte-mutated representation rejected");
    const char incorrectDigest[] = "aa7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    Expect(!MatchesSha256(shaTestVector, sizeof(shaTestVector), sizeof(shaTestVector), incorrectDigest),
           "incorrect SHA-256 rejected");

    WebPagePool_t mismatchedAssetPool = {};
    const char staleBundleId[] = "fbe7f2a9033e8f957d0460ec8ff929298e2073de3e292037846237cab6422701";
    static_assert(sizeof(staleBundleId) == kBundleIdCapacity, "stale bundle ID storage size");
    std::memcpy(mismatchedAssetPool.viewer_bundle_id, staleBundleId, sizeof(staleBundleId));
    Expect(!IsExpectedBundleId(mismatchedAssetPool.viewer_bundle_id, sizeof(mismatchedAssetPool.viewer_bundle_id)),
           "stale fbe7 bundle ID rejected");
    gRegisteredRouteCount = 0U;
    Expect(!VIEWER_HTTP_ROUTES::Register(reinterpret_cast<httpd_handle_t>(1), &mismatchedAssetPool, DisplayProfile::Voltage),
           "mismatched AssetPool registration fails closed");
    Expect(gRegisteredRouteCount == 0U, "mismatched AssetPool registers no Viewer routes");

    const char stableBundleId[] = "4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af";
    static_assert(sizeof(stableBundleId) == kBundleIdCapacity, "stable bundle ID storage size");
    std::memcpy(mismatchedAssetPool.viewer_bundle_id, stableBundleId, sizeof(stableBundleId));
    Expect(!VIEWER_HTTP_ROUTES::Register(reinterpret_cast<httpd_handle_t>(1), &mismatchedAssetPool, DisplayProfile::Voltage),
           "stable v2.0.0 bundle rejected");
    Expect(gRegisteredRouteCount == 0U, "stable bundle registers no Viewer routes");

    const char earlierBundleId[] = "6fe4991f3dcea5793b4b19736e4ab9c3ca39869c59e789776abae5a5d84733ca";
    static_assert(sizeof(earlierBundleId) == kBundleIdCapacity, "earlier bundle ID storage size");
    std::memcpy(mismatchedAssetPool.viewer_bundle_id, earlierBundleId, sizeof(earlierBundleId));
    Expect(!IsExpectedBundleId(mismatchedAssetPool.viewer_bundle_id, sizeof(mismatchedAssetPool.viewer_bundle_id)),
           "stale 6fe499 bundle ID rejected");

    std::memcpy(mismatchedAssetPool.viewer_bundle_id, kViewerBundleId, kBundleIdCapacity);
    gRegisteredRouteCount = 0U;
    Expect(!VIEWER_HTTP_ROUTES::Register(reinterpret_cast<httpd_handle_t>(1), &mismatchedAssetPool, DisplayProfile::Voltage),
           "correct bundle ID with incorrect representation hashes fails closed");
    Expect(gRegisteredRouteCount == 0U, "incorrect representation hashes register no Viewer routes");

    std::cout << "PASS: final Viewer intake contract (host only)\n";
    return 0;
}
