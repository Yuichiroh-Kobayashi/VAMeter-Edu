#pragma once

#include <cstddef>
#include <cstdint>
#include "assets/static_asset_types.h"

namespace ASSET_POOL_LAYOUT
{
    static const std::size_t kAssetPoolPartitionBytes = 2097152U;
    static const std::size_t kTrailerReserveBytes = 256U;
    static const std::size_t kTrailerOffset = kAssetPoolPartitionBytes - kTrailerReserveBytes;
    static const std::size_t kTrailerUsedBytes = 76U;
    static const std::uint16_t kTrailerFormatVersion = 1U;
    static const std::uint16_t kStaticAssetLayoutVersion = 1U;
    static const std::uint16_t kViewerLayoutVersion = 1U;
    static const std::size_t kStaticAssetBytes = sizeof(StaticAsset_t);
    static const std::size_t kWebPageOffset = offsetof(StaticAsset_t, WebPage);
    static const std::size_t kViewerMemberCount = 5U;

    struct MemberRange
    {
        std::size_t offset;
        std::size_t capacity;
    };

    // This is compile authority, never decoded from the container.
    static constexpr MemberRange kViewerMembers[kViewerMemberCount] = {
        {kWebPageOffset + offsetof(WebPagePool_t, viewer_index_html), sizeof(WebPagePool_t::viewer_index_html)},
        {kWebPageOffset + offsetof(WebPagePool_t, viewer_asset_manifest), sizeof(WebPagePool_t::viewer_asset_manifest)},
        {kWebPageOffset + offsetof(WebPagePool_t, viewer_css_gzip), sizeof(WebPagePool_t::viewer_css_gzip)},
        {kWebPageOffset + offsetof(WebPagePool_t, viewer_js_gzip), sizeof(WebPagePool_t::viewer_js_gzip)},
        {kWebPageOffset + offsetof(WebPagePool_t, viewer_bundle_id), sizeof(WebPagePool_t::viewer_bundle_id)},
    };

    constexpr bool FitsRange(std::size_t offset, std::size_t length, std::size_t limit)
    {
        return offset <= limit && length <= limit - offset;
    }

    enum class Result
    {
        Ok,
        InvalidBuffer,
        PartitionSizeMismatch,
        TrailerSizeMismatch,
        MagicMismatch,
        FormatVersionMismatch,
        HeaderSizeMismatch,
        TrailerCrcMismatch,
        StaticLayoutMismatch,
        ViewerLayoutMismatch,
        StaticAssetSizeMismatch,
        WebPageOffsetMismatch,
        MemberCountMismatch,
        MemberOffsetMismatch,
        MemberCapacityMismatch,
        MemberRangeInvalid,
        MemberOrderInvalid,
        ReservedNonzero,
        StaticAssetCrcMismatch,
    };

    const char* ResultName(Result result);
    // Caller supplies a readable range. A null pointer is permitted only for zero bytes.
    std::uint32_t Crc32(const void* data, std::size_t bytes);
    Result EncodeTrailer(const void* staticAsset, std::size_t bytes, std::uint8_t* trailer, std::size_t trailerBytes);
    Result ValidateTrailer(const std::uint8_t* trailer, std::size_t trailerBytes);
    // Trailer shape is checked before any access to staticAsset. The CRC read length is
    // always kStaticAssetBytes, never the size declared in the trailer.
    Result
    ValidateStaticAsset(const void* staticAsset, std::size_t bytes, const std::uint8_t* trailer, std::size_t trailerBytes);
    Result ValidateContainer(const void* container, std::size_t bytes);
} // namespace ASSET_POOL_LAYOUT
