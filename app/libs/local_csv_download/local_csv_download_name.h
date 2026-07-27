/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace LOCAL_CSV_DOWNLOAD
{
    const std::size_t kMaxRecordNameLength = 64;

    bool IsAllowedRecordName(const std::string& recordName);
    bool ParseRecordId(const std::string& recordName, std::uint32_t& recordId);
    bool IsCurrentRecordName(const std::string& recordName);
    bool IsLegacyRecordName(const std::string& recordName);
    bool NextRecordId(const std::vector<std::string>& recordNames, std::uint32_t& nextRecordId);
    bool HasEnoughRecordingSpace(bool storageInfoAvailable,
                                 std::uint64_t freeBytes,
                                 std::uint64_t requiredBytes);
    bool DecodeUrlComponentOnce(const std::string& encodedName, std::string& decodedName);
}
