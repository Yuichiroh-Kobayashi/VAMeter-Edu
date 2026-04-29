/*
 * SPDX-License-Identifier: MIT
 */
#include "local_csv_download_name.h"

#include <cctype>
#include <sys/stat.h>

namespace LOCAL_CSV_DOWNLOAD
{
    namespace
    {
        bool HasAllowedPrefix(const std::string& recordName, std::size_t& digitStart)
        {
            if (recordName.compare(0, 4, "REC-") == 0)
            {
                digitStart = 4;
                return true;
            }

            if (recordName.compare(0, 3, "MO-") == 0)
            {
                digitStart = 3;
                return true;
            }

            return false;
        }
    } // namespace

    bool IsAllowedRecordName(const std::string& recordName)
    {
        if (recordName.empty())
            return false;

        if (recordName.find('/') != std::string::npos || recordName.find('\\') != std::string::npos)
            return false;

        if (recordName.find("..") != std::string::npos || recordName.find('?') != std::string::npos)
            return false;

        static const std::string suffix = ".csv";
        if (recordName.size() <= suffix.size() || recordName.compare(recordName.size() - suffix.size(), suffix.size(), suffix) != 0)
            return false;

        std::size_t digitStart = 0;
        if (!HasAllowedPrefix(recordName, digitStart))
            return false;

        const std::size_t digitEnd = recordName.size() - suffix.size();
        if (digitEnd <= digitStart)
            return false;

        for (std::size_t i = digitStart; i < digitEnd; ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(recordName[i])))
                return false;
        }

        return true;
    }

    RecordFileStatus CheckClosedRecordFileForDownload(const std::string& recordName, const std::string& filePath)
    {
        if (!IsAllowedRecordName(recordName))
            return RecordFileStatus::InvalidName;

        struct stat fileStat;
        if (stat(filePath.c_str(), &fileStat) != 0)
            return RecordFileStatus::StatFailed;

        if (fileStat.st_size <= 0)
            return RecordFileStatus::Empty;

        return RecordFileStatus::Ready;
    }
} // namespace LOCAL_CSV_DOWNLOAD
