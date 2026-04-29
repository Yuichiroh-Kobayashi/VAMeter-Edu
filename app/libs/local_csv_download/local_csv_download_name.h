/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <string>

namespace LOCAL_CSV_DOWNLOAD
{
    enum class RecordFileStatus
    {
        Ready,
        InvalidName,
        StatFailed,
        Empty,
    };

    bool IsAllowedRecordName(const std::string& recordName);
    RecordFileStatus CheckClosedRecordFileForDownload(const std::string& recordName, const std::string& filePath);
}
