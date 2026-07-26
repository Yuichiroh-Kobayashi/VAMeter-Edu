/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <string>

namespace LOCAL_CSV_DOWNLOAD
{
    const std::size_t kMaxRecordNameLength = 64;

    bool IsAllowedRecordName(const std::string& recordName);
    bool DecodeUrlComponentOnce(const std::string& encodedName, std::string& decodedName);
}
