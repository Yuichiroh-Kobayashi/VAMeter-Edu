#include "asset_pool_layout.h"
#include "libs/viewer_asset_contract/viewer_asset_contract.h"
#include <cstring>
#include <type_traits>

namespace ASSET_POOL_LAYOUT
{
    static_assert(std::is_standard_layout<StaticAsset_t>::value, "StaticAsset_t must support offsetof");
    static_assert(std::is_standard_layout<WebPagePool_t>::value, "WebPagePool_t must support offsetof");
    static_assert(std::is_trivially_copyable<StaticAsset_t>::value, "StaticAsset_t must support byte loading");
    static_assert(kStaticAssetBytes + kTrailerReserveBytes <= kAssetPoolPartitionBytes, "AssetPool exceeds partition");
    static_assert(kTrailerUsedBytes <= kTrailerReserveBytes, "Trailer exceeds reserve");
    static_assert(kStaticAssetBytes == 1655972U, "Review and version any layout-1 size change");
    static_assert(sizeof(WebPagePool_t) == 115498U, "Review and version any Viewer layout-1 size change");

#define CHECK_MEMBER(index, expected)                                                                                          \
    static_assert(kViewerMembers[index].capacity == expected, "Viewer slot must match compile contract");                      \
    static_assert(FitsRange(kViewerMembers[index].offset, kViewerMembers[index].capacity, kStaticAssetBytes),                  \
                  "Viewer member exceeds StaticAsset_t")
    CHECK_MEMBER(0, VIEWER_ASSET_CONTRACT::kIndexBytes);
    CHECK_MEMBER(1, VIEWER_ASSET_CONTRACT::kManifestBytes);
    CHECK_MEMBER(2, VIEWER_ASSET_CONTRACT::kCssGzipBytes);
    CHECK_MEMBER(3, VIEWER_ASSET_CONTRACT::kJsGzipBytes);
    CHECK_MEMBER(4, VIEWER_ASSET_CONTRACT::kBundleIdCapacity);
#undef CHECK_MEMBER

    namespace
    {
        const std::uint8_t kMagic[8] = {'V', 'A', 'M', 'E', 'A', 'P', 'L', '1'};

        std::uint16_t Read16(const std::uint8_t* p)
        {
            return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8U));
        }

        std::uint32_t Read32(const std::uint8_t* p)
        {
            return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8U) |
                   (static_cast<std::uint32_t>(p[2]) << 16U) | (static_cast<std::uint32_t>(p[3]) << 24U);
        }

        void Write16(std::uint8_t* p, std::uint16_t value)
        {
            p[0] = static_cast<std::uint8_t>(value);
            p[1] = static_cast<std::uint8_t>(value >> 8U);
        }

        void Write32(std::uint8_t* p, std::uint32_t value)
        {
            for (unsigned i = 0; i < 4U; ++i)
                p[i] = static_cast<std::uint8_t>(value >> (8U * i));
        }
    } // namespace

    std::uint32_t Crc32(const void* data, std::size_t bytes)
    {
        const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
        std::uint32_t crc = 0xFFFFFFFFU;
        for (std::size_t i = 0; i < bytes; ++i)
        {
            crc ^= p[i];
            for (unsigned bit = 0; bit < 8U; ++bit)
                crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
        return crc ^ 0xFFFFFFFFU;
    }

    Result EncodeTrailer(const void* staticAsset, std::size_t bytes, std::uint8_t* trailer, std::size_t trailerBytes)
    {
        if (trailerBytes != kTrailerReserveBytes)
            return Result::TrailerSizeMismatch;
        if (bytes != kStaticAssetBytes)
            return Result::StaticAssetSizeMismatch;
        if (staticAsset == nullptr || trailer == nullptr)
            return Result::InvalidBuffer;
        std::memset(trailer, 0, kTrailerReserveBytes);
        std::memcpy(trailer, kMagic, sizeof(kMagic));
        Write16(trailer + 8, kTrailerFormatVersion);
        Write16(trailer + 10, kTrailerUsedBytes);
        Write16(trailer + 12, kStaticAssetLayoutVersion);
        Write16(trailer + 14, kViewerLayoutVersion);
        Write32(trailer + 16, kStaticAssetBytes);
        Write32(trailer + 20, kWebPageOffset);
        Write32(trailer + 24, kViewerMemberCount);
        for (std::size_t i = 0; i < kViewerMemberCount; ++i)
        {
            Write32(trailer + 28 + 8 * i, kViewerMembers[i].offset);
            Write32(trailer + 32 + 8 * i, kViewerMembers[i].capacity);
        }
        Write32(trailer + 68, Crc32(staticAsset, kStaticAssetBytes));
        Write32(trailer + 72, Crc32(trailer, 72));
        return Result::Ok;
    }

    Result ValidateTrailer(const std::uint8_t* trailer, std::size_t trailerBytes)
    {
        if (trailerBytes != kTrailerReserveBytes)
            return Result::TrailerSizeMismatch;
        if (trailer == nullptr)
            return Result::InvalidBuffer;
        if (std::memcmp(trailer, kMagic, sizeof(kMagic)) != 0)
            return Result::MagicMismatch;
        if (Read16(trailer + 8) != kTrailerFormatVersion)
            return Result::FormatVersionMismatch;
        if (Read16(trailer + 10) != kTrailerUsedBytes)
            return Result::HeaderSizeMismatch;
        if (Read32(trailer + 72) != Crc32(trailer, 72))
            return Result::TrailerCrcMismatch;
        if (Read16(trailer + 12) != kStaticAssetLayoutVersion)
            return Result::StaticLayoutMismatch;
        if (Read16(trailer + 14) != kViewerLayoutVersion)
            return Result::ViewerLayoutMismatch;
        if (Read32(trailer + 16) != kStaticAssetBytes)
            return Result::StaticAssetSizeMismatch;
        if (Read32(trailer + 20) != kWebPageOffset)
            return Result::WebPageOffsetMismatch;
        if (Read32(trailer + 24) != kViewerMemberCount)
            return Result::MemberCountMismatch;

        std::size_t previousOffset = 0;
        std::size_t previousEnd = kWebPageOffset;
        for (std::size_t i = 0; i < kViewerMemberCount; ++i)
        {
            const std::size_t offset = Read32(trailer + 28 + 8 * i);
            const std::size_t capacity = Read32(trailer + 32 + 8 * i);
            if (offset != kViewerMembers[i].offset)
                return Result::MemberOffsetMismatch;
            if (capacity != kViewerMembers[i].capacity)
                return Result::MemberCapacityMismatch;
            if (capacity == 0 || !FitsRange(offset, capacity, kStaticAssetBytes))
                return Result::MemberRangeInvalid;
            if (offset <= previousOffset || offset < previousEnd)
                return Result::MemberOrderInvalid;
            previousOffset = offset;
            previousEnd = offset + capacity; // Safe only after FitsRange.
        }
        for (std::size_t i = kTrailerUsedBytes; i < kTrailerReserveBytes; ++i)
            if (trailer[i] != 0)
                return Result::ReservedNonzero;
        return Result::Ok;
    }

    Result
    ValidateStaticAsset(const void* staticAsset, std::size_t bytes, const std::uint8_t* trailer, std::size_t trailerBytes)
    {
        const Result shape = ValidateTrailer(trailer, trailerBytes);
        if (shape != Result::Ok)
            return shape;
        if (bytes != kStaticAssetBytes)
            return Result::StaticAssetSizeMismatch;
        if (staticAsset == nullptr)
            return Result::InvalidBuffer;
        if (Crc32(staticAsset, kStaticAssetBytes) != Read32(trailer + 68))
            return Result::StaticAssetCrcMismatch;
        return Result::Ok;
    }

    Result ValidateContainer(const void* container, std::size_t bytes)
    {
        if (bytes != kAssetPoolPartitionBytes)
            return Result::PartitionSizeMismatch;
        if (container == nullptr)
            return Result::InvalidBuffer;
        std::uint8_t trailer[kTrailerReserveBytes];
        std::memcpy(trailer, static_cast<const std::uint8_t*>(container) + kTrailerOffset, sizeof(trailer));
        return ValidateStaticAsset(container, kStaticAssetBytes, trailer, sizeof(trailer));
    }

    const char* ResultName(Result result)
    {
#define RESULT_NAME(name)                                                                                                      \
    case Result::name:                                                                                                         \
        return #name
        switch (result)
        {
            RESULT_NAME(Ok);
            RESULT_NAME(InvalidBuffer);
            RESULT_NAME(PartitionSizeMismatch);
            RESULT_NAME(TrailerSizeMismatch);
            RESULT_NAME(MagicMismatch);
            RESULT_NAME(FormatVersionMismatch);
            RESULT_NAME(HeaderSizeMismatch);
            RESULT_NAME(TrailerCrcMismatch);
            RESULT_NAME(StaticLayoutMismatch);
            RESULT_NAME(ViewerLayoutMismatch);
            RESULT_NAME(StaticAssetSizeMismatch);
            RESULT_NAME(WebPageOffsetMismatch);
            RESULT_NAME(MemberCountMismatch);
            RESULT_NAME(MemberOffsetMismatch);
            RESULT_NAME(MemberCapacityMismatch);
            RESULT_NAME(MemberRangeInvalid);
            RESULT_NAME(MemberOrderInvalid);
            RESULT_NAME(ReservedNonzero);
            RESULT_NAME(StaticAssetCrcMismatch);
        }
#undef RESULT_NAME
        return "UnknownLayoutFailure";
    }
} // namespace ASSET_POOL_LAYOUT
