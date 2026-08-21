/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <mutex>
#include <string>

namespace LOCAL_CSV_DOWNLOAD
{
    struct DownloadSelectionSnapshot
    {
        std::string name;
        std::string path;
    };

    class DownloadSelection
    {
    public:
        void set(const std::string& name, const std::string& path);
        DownloadSelectionSnapshot snapshot() const;
        void clear();

    private:
        mutable std::mutex _mutex;
        DownloadSelectionSnapshot _selection;
    };
}
