#include "libs/asset_pool_layout/asset_pool_layout.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#if defined(__unix__)
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace ASSET_POOL_LAYOUT;
namespace
{
    unsigned checks = 0;
    void Check(bool ok, const std::string& name)
    {
        if (!ok)
        {
            std::fprintf(stderr, "FAIL %s\n", name.c_str());
            std::exit(EXIT_FAILURE);
        }
        ++checks;
        std::printf("PASS %s\n", name.c_str());
    }
    void Write(std::uint8_t* p, unsigned width, std::uint32_t value)
    {
        for (unsigned i = 0; i < width; ++i)
            p[i] = static_cast<std::uint8_t>(value >> (8U * i));
    }
    void Seal(std::array<std::uint8_t, kTrailerReserveBytes>& trailer)
    {
        Write(trailer.data() + 72, 4, Crc32(trailer.data(), 72));
    }
} // namespace

int main()
{
    Check(Crc32(nullptr, 0) == 0U, "crc32_empty");
    Check(Crc32("123456789", 9) == 0xCBF43926U, "crc32_123456789");
    std::vector<std::uint8_t> asset(kStaticAssetBytes, 0xA5);
    std::array<std::uint8_t, kTrailerReserveBytes> good = {}, second = {};
    Check(EncodeTrailer(asset.data(), asset.size(), good.data(), good.size()) == Result::Ok, "encode_valid");
    Check(EncodeTrailer(asset.data(), asset.size(), second.data(), second.size()) == Result::Ok && good == second,
          "encode_deterministic_256_bytes");
    Check(ValidateTrailer(good.data(), good.size()) == Result::Ok, "current_trailer");
    Check(ValidateStaticAsset(asset.data(), asset.size(), good.data(), good.size()) == Result::Ok, "current_static_asset");
    Check(std::memcmp(good.data(), "VAMEAPL1", 8) == 0 && good[8] == 1 && good[9] == 0 && good[10] == 76 && good[11] == 0,
          "magic_and_little_endian_header");
    const auto bad = [&](std::size_t offset, unsigned width, std::uint32_t value, Result reason, const std::string& name)
    {
        auto t = good;
        Write(t.data() + offset, width, value);
        Seal(t);
        Check(ValidateTrailer(t.data(), t.size()) == reason &&
                  ValidateStaticAsset(nullptr, kStaticAssetBytes, t.data(), t.size()) == reason,
              name + "_before_any_asset_read");
    };
    bad(0, 1, 0, Result::MagicMismatch, "wrong_magic");
    for (const auto value : {0U, 2U, 0xFFFFU})
        bad(8, 2, value, Result::FormatVersionMismatch, "format_" + std::to_string(value));
    for (const auto value : {75U, 77U, 0U, 0xFFFFU})
        bad(10, 2, value, Result::HeaderSizeMismatch, "header_" + std::to_string(value));
    Check(kStaticAssetLayoutVersion == 2U && kViewerLayoutVersion == 2U && kStaticAssetBytes == 1660612U &&
              sizeof(WebPagePool_t) == 120141U,
          "exact_layout_v2_authority");
    for (const auto value : {0U, 1U, 3U, 0xFFFFU})
    {
        bad(12, 2, value, Result::StaticLayoutMismatch, "static_layout_" + std::to_string(value));
        bad(14, 2, value, Result::ViewerLayoutMismatch, "viewer_layout_" + std::to_string(value));
    }
    // A v1 member table is still rejected if its version fields are forged as v2.
    bad(48, 4, 2385U, Result::MemberCapacityMismatch, "v1_css_rejected_by_exact_capacity");
    bad(52, 4, 1630095U, Result::MemberOffsetMismatch, "v1_js_rejected_by_exact_offset");
    bad(56, 4, 25809U, Result::MemberCapacityMismatch, "v1_js_rejected_by_exact_capacity");
    bad(60, 4, 1655904U, Result::MemberOffsetMismatch, "v1_bundle_rejected_by_exact_offset");
    for (const auto value : {0U, static_cast<unsigned>(kStaticAssetBytes + 1U), 0xFFFFFFFFU})
        bad(16, 4, value, Result::StaticAssetSizeMismatch, "declared_size_" + std::to_string(value));
    bad(20, 4, kWebPageOffset + 1, Result::WebPageOffsetMismatch, "webpage_offset");
    for (const auto value : {0U, 4U, 6U, 0xFFFFFFFFU})
        bad(24, 4, value, Result::MemberCountMismatch, "member_count_" + std::to_string(value));
    for (std::size_t i = 0; i < kViewerMemberCount; ++i)
    {
        bad(28 + 8 * i, 4, kViewerMembers[i].offset + 1, Result::MemberOffsetMismatch, "member_offset_" + std::to_string(i));
        bad(32 + 8 * i,
            4,
            kViewerMembers[i].capacity + 1,
            Result::MemberCapacityMismatch,
            "member_capacity_" + std::to_string(i));
    }
    bad(28, 4, kStaticAssetBytes + 1, Result::MemberOffsetMismatch, "out_of_range_rejected_by_exact_offset");
    bad(28, 4, 0xFFFFFFF0U, Result::MemberOffsetMismatch, "addition_overflow_rejected_by_exact_offset");
    bad(32, 4, 0xFFFFFFFFU, Result::MemberCapacityMismatch, "addition_overflow_rejected_by_exact_capacity");
    bad(36, 4, kViewerMembers[0].offset + 1, Result::MemberOffsetMismatch, "overlap_rejected_by_exact_offset");
    bad(36, 4, kViewerMembers[0].offset, Result::MemberOffsetMismatch, "non_increasing_rejected_by_exact_offset");
    bad(36, 4, kViewerMembers[0].offset - 1, Result::MemberOffsetMismatch, "reverse_order_rejected_by_exact_offset");
    for (std::size_t i = kTrailerUsedBytes; i < kTrailerReserveBytes; ++i)
    {
        auto t = good;
        t[i] = 1;
        Check(ValidateStaticAsset(nullptr, kStaticAssetBytes, t.data(), t.size()) == Result::ReservedNonzero,
              "reserved_byte_" + std::to_string(i));
    }
    auto corrupt = good;
    corrupt[72] ^= 1;
    Check(ValidateStaticAsset(nullptr, kStaticAssetBytes, corrupt.data(), corrupt.size()) == Result::TrailerCrcMismatch,
          "trailer_crc_mismatch_before_asset_read");
    asset[0] ^= 1;
    Check(ValidateStaticAsset(asset.data(), asset.size(), good.data(), good.size()) == Result::StaticAssetCrcMismatch,
          "static_crc_mismatch");
    asset[0] ^= 1;
    auto old = good;
    old.fill(0xFF);
    Check(ValidateStaticAsset(nullptr, kStaticAssetBytes, old.data(), old.size()) == Result::MagicMismatch,
          "new_firmware_old_pool_tail_fails_magic_before_use");
    Check(ValidateTrailer(nullptr, 0) == Result::TrailerSizeMismatch, "short_trailer_before_read");
    Check(ValidateTrailer(nullptr, kTrailerReserveBytes) == Result::InvalidBuffer, "null_trailer");
    Check(ValidateStaticAsset(nullptr, kStaticAssetBytes, good.data(), good.size()) == Result::InvalidBuffer, "null_asset");
    Check(ValidateStaticAsset(nullptr, 0, good.data(), good.size()) == Result::StaticAssetSizeMismatch, "short_asset");
    Check(ValidateContainer(nullptr, kStaticAssetBytes) == Result::PartitionSizeMismatch, "legacy_short_container");
    Check(ValidateContainer(nullptr, kAssetPoolPartitionBytes + 1) == Result::PartitionSizeMismatch, "oversize_container");
    Check(ValidateContainer(nullptr, kAssetPoolPartitionBytes) == Result::InvalidBuffer, "null_container");
    Check(EncodeTrailer(nullptr, 0, nullptr, 0) == Result::TrailerSizeMismatch, "encode_short_output");
    Check(EncodeTrailer(nullptr, 0, good.data(), good.size()) == Result::StaticAssetSizeMismatch, "encode_short_input");
    Check(EncodeTrailer(nullptr, kStaticAssetBytes, good.data(), good.size()) == Result::InvalidBuffer, "encode_null_input");
    Check(!FitsRange(std::numeric_limits<std::size_t>::max() - 1, 4, kStaticAssetBytes) &&
              !FitsRange(1, std::numeric_limits<std::size_t>::max(), kStaticAssetBytes) &&
              FitsRange(kStaticAssetBytes, 0, kStaticAssetBytes),
          "overflow_safe_range_predicate");
    std::vector<std::uint8_t> container(kAssetPoolPartitionBytes, 0);
    std::memcpy(container.data(), asset.data(), asset.size());
    std::memcpy(container.data() + kTrailerOffset, good.data(), good.size());
    Check(ValidateContainer(container.data(), container.size()) == Result::Ok, "whole_container_valid");
    // Layout/CRC validation deliberately does not enforce Viewer identity (Tier 2).
    container[kViewerMembers[0].offset] ^= 1;
    Check(EncodeTrailer(container.data(), kStaticAssetBytes, container.data() + kTrailerOffset, kTrailerReserveBytes) ==
                  Result::Ok &&
              ValidateContainer(container.data(), container.size()) == Result::Ok,
          "tier2_identity_is_not_tier1");
#if defined(__unix__)
    const std::size_t page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    const std::size_t rounded = ((kStaticAssetBytes + page - 1) / page) * page;
    void* mapping = mmap(nullptr, rounded + page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Check(mapping != MAP_FAILED, "guard_page_allocation");
    auto end = static_cast<std::uint8_t*>(mapping) + rounded;
    Check(mprotect(end, page, PROT_NONE) == 0, "guard_page_protected");
    auto bounded = end - kStaticAssetBytes;
    std::memcpy(bounded, asset.data(), asset.size());
    Check(ValidateStaticAsset(bounded, kStaticAssetBytes, good.data(), good.size()) == Result::Ok,
          "crc_stops_exactly_at_compile_bound");
    auto oversized = good;
    Write(oversized.data() + 16, 4, 0xFFFFFFFFU);
    Seal(oversized);
    Check(ValidateStaticAsset(end, kStaticAssetBytes, oversized.data(), oversized.size()) == Result::StaticAssetSizeMismatch,
          "ffff_declared_length_never_reads_protected_memory");
    Check(munmap(mapping, rounded + page) == 0, "guard_page_release");
#endif
    std::printf("asset_pool_layout_test: %u checks PASS\n", checks);
}
