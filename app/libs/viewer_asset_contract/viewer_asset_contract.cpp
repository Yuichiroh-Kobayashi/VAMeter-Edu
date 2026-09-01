#include "viewer_asset_contract.h"

#include <cstdio>
#include <cstring>

namespace VIEWER_ASSET_CONTRACT
{
    const char kViewerBundleId[] = "4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af";
    const char kIndexSha256[] = "88c39f443ef49d477d558f86dfd4b02345ed49e5a0d1123ce92538b7dafc54b7";
    const char kManifestSha256[] = "4422530b6e1ba9549dd4bef2e3bb2c183d8fced49ed2d8d695d2a04a4aa7c2af";
    const char kCssGzipSha256[] = "250905db503bf774bcab87f29e44ceddd63c949e798cc72c55864b933a8cfafb";
    const char kJsGzipSha256[] = "46bafb3d23345cd8dc48533c4c59c595ef6b0ae5cf051158dae509a20f56cbe4";

    const char kRootRoute[] = "/";
    const char kViewerRoute[] = "/viewer/";
    const char kManifestRoute[] = "/viewer/asset-manifest.json";
    const char kCssRoute[] = "/viewer/assets/app.250905db503bf774bcab87f29e44ceddd63c949e798cc72c55864b933a8cfafb.css";
    const char kJsRoute[] = "/viewer/assets/app.46bafb3d23345cd8dc48533c4c59c595ef6b0ae5cf051158dae509a20f56cbe4.js";
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
        std::uint32_t RotateRight(std::uint32_t value, std::uint32_t count)
        {
            return (value >> count) | (value << (32U - count));
        }

        void TransformSha256(const std::uint8_t block[64], std::uint32_t state[8])
        {
            static const std::uint32_t kRoundConstants[64] = {
                0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
                0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
                0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
                0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
                0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
                0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
                0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
                0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
            };
            std::uint32_t words[64] = {};
            for (std::size_t index = 0; index < 16U; ++index)
            {
                const std::size_t offset = index * 4U;
                words[index] = (static_cast<std::uint32_t>(block[offset]) << 24U) |
                               (static_cast<std::uint32_t>(block[offset + 1U]) << 16U) |
                               (static_cast<std::uint32_t>(block[offset + 2U]) << 8U) |
                               static_cast<std::uint32_t>(block[offset + 3U]);
            }
            for (std::size_t index = 16U; index < 64U; ++index)
            {
                const std::uint32_t sigma0 =
                    RotateRight(words[index - 15U], 7U) ^ RotateRight(words[index - 15U], 18U) ^ (words[index - 15U] >> 3U);
                const std::uint32_t sigma1 =
                    RotateRight(words[index - 2U], 17U) ^ RotateRight(words[index - 2U], 19U) ^ (words[index - 2U] >> 10U);
                words[index] = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
            }

            std::uint32_t a = state[0];
            std::uint32_t b = state[1];
            std::uint32_t c = state[2];
            std::uint32_t d = state[3];
            std::uint32_t e = state[4];
            std::uint32_t f = state[5];
            std::uint32_t g = state[6];
            std::uint32_t h = state[7];
            for (std::size_t index = 0; index < 64U; ++index)
            {
                const std::uint32_t sum1 = RotateRight(e, 6U) ^ RotateRight(e, 11U) ^ RotateRight(e, 25U);
                const std::uint32_t choose = (e & f) ^ ((~e) & g);
                const std::uint32_t temporary1 = h + sum1 + choose + kRoundConstants[index] + words[index];
                const std::uint32_t sum0 = RotateRight(a, 2U) ^ RotateRight(a, 13U) ^ RotateRight(a, 22U);
                const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                const std::uint32_t temporary2 = sum0 + majority;
                h = g;
                g = f;
                f = e;
                e = d + temporary1;
                d = c;
                c = b;
                b = a;
                a = temporary1 + temporary2;
            }
            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            state[4] += e;
            state[5] += f;
            state[6] += g;
            state[7] += h;
        }

        void ComputeSha256(const std::uint8_t* bytes, std::size_t bytesCount, std::uint8_t digest[32])
        {
            std::uint32_t state[8] = {
                0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
            std::size_t offset = 0U;
            while (bytesCount - offset >= 64U)
            {
                TransformSha256(bytes + offset, state);
                offset += 64U;
            }

            std::uint8_t finalBlocks[128] = {};
            const std::size_t remaining = bytesCount - offset;
            if (remaining != 0U)
                std::memcpy(finalBlocks, bytes + offset, remaining);
            finalBlocks[remaining] = 0x80U;
            const std::size_t finalBytes = remaining < 56U ? 64U : 128U;
            const std::uint64_t bitCount = static_cast<std::uint64_t>(bytesCount) * 8U;
            for (std::size_t index = 0; index < 8U; ++index)
                finalBlocks[finalBytes - 1U - index] = static_cast<std::uint8_t>(bitCount >> (index * 8U));
            TransformSha256(finalBlocks, state);
            if (finalBytes == 128U)
                TransformSha256(finalBlocks + 64U, state);

            for (std::size_t index = 0; index < 8U; ++index)
            {
                digest[index * 4U] = static_cast<std::uint8_t>(state[index] >> 24U);
                digest[index * 4U + 1U] = static_cast<std::uint8_t>(state[index] >> 16U);
                digest[index * 4U + 2U] = static_cast<std::uint8_t>(state[index] >> 8U);
                digest[index * 4U + 3U] = static_cast<std::uint8_t>(state[index]);
            }
        }

        bool DecodeHexNibble(char value, std::uint8_t* output)
        {
            if (value >= '0' && value <= '9')
                *output = static_cast<std::uint8_t>(value - '0');
            else if (value >= 'a' && value <= 'f')
                *output = static_cast<std::uint8_t>(value - 'a' + 10);
            else
                return false;
            return true;
        }

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

    bool MatchesSha256(const std::uint8_t* bytes, std::size_t bytesCount, std::size_t expectedBytes, const char* expectedSha256)
    {
        if (bytes == nullptr || bytesCount != expectedBytes || expectedSha256 == nullptr || std::strlen(expectedSha256) != 64U)
            return false;

        std::uint8_t expectedDigest[32] = {};
        for (std::size_t index = 0; index < 32U; ++index)
        {
            std::uint8_t high = 0U;
            std::uint8_t low = 0U;
            if (!DecodeHexNibble(expectedSha256[index * 2U], &high) || !DecodeHexNibble(expectedSha256[index * 2U + 1U], &low))
                return false;
            expectedDigest[index] = static_cast<std::uint8_t>((high << 4U) | low);
        }

        std::uint8_t observedDigest[32] = {};
        ComputeSha256(bytes, bytesCount, observedDigest);
        return std::memcmp(observedDigest, expectedDigest, sizeof(observedDigest)) == 0;
    }
} // namespace VIEWER_ASSET_CONTRACT
