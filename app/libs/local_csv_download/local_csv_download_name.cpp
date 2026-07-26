/*
 * SPDX-License-Identifier: MIT
 */
#include "local_csv_download_name.h"

namespace LOCAL_CSV_DOWNLOAD
{
    namespace
    {
        int HexValue(char character)
        {
            if (character >= '0' && character <= '9')
                return character - '0';
            if (character >= 'a' && character <= 'f')
                return character - 'a' + 10;
            if (character >= 'A' && character <= 'F')
                return character - 'A' + 10;
            return -1;
        }
    } // namespace

    bool IsAllowedRecordName(const std::string& recordName)
    {
        static const std::string prefix = "REC-";
        static const std::string suffix = ".csv";

        if (recordName.empty() || recordName.size() > kMaxRecordNameLength)
            return false;

        for (std::size_t i = 0; i < recordName.size(); ++i)
        {
            const unsigned char character = static_cast<unsigned char>(recordName[i]);
            if (character < 0x20 || character == 0x7f)
                return false;
        }

        if (recordName.find('/') != std::string::npos || recordName.find('\\') != std::string::npos)
            return false;

        if (recordName.find("..") != std::string::npos)
            return false;

        if (recordName.size() <= prefix.size() + suffix.size())
            return false;

        if (recordName.compare(0, prefix.size(), prefix) != 0)
            return false;

        if (recordName.compare(recordName.size() - suffix.size(), suffix.size(), suffix) != 0)
            return false;

        const std::size_t digitEnd = recordName.size() - suffix.size();
        for (std::size_t i = prefix.size(); i < digitEnd; ++i)
        {
            if (recordName[i] < '0' || recordName[i] > '9')
                return false;
        }

        return true;
    }

    bool DecodeUrlComponentOnce(const std::string& encodedName, std::string& decodedName)
    {
        const std::size_t maxEncodedLength = kMaxRecordNameLength * 3;
        decodedName.clear();

        if (encodedName.empty() || encodedName.size() > maxEncodedLength)
            return false;

        decodedName.reserve(encodedName.size());
        for (std::size_t i = 0; i < encodedName.size(); ++i)
        {
            if (encodedName[i] != '%')
            {
                decodedName.push_back(encodedName[i]);
            }
            else
            {
                if (i + 2 >= encodedName.size())
                {
                    decodedName.clear();
                    return false;
                }

                const int high = HexValue(encodedName[i + 1]);
                const int low = HexValue(encodedName[i + 2]);
                if (high < 0 || low < 0)
                {
                    decodedName.clear();
                    return false;
                }

                decodedName.push_back(static_cast<char>((high << 4) | low));
                i += 2;
            }

            if (decodedName.size() > kMaxRecordNameLength)
            {
                decodedName.clear();
                return false;
            }
        }

        return true;
    }
}
