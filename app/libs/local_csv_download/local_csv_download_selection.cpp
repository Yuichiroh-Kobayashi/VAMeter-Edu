/*
 * SPDX-License-Identifier: MIT
 */
#include "local_csv_download_selection.h"

namespace LOCAL_CSV_DOWNLOAD
{
    void DownloadSelection::set(const std::string& name, const std::string& path)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _selection.name = name;
        _selection.path = path;
    }

    DownloadSelectionSnapshot DownloadSelection::snapshot() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _selection;
    }

    void DownloadSelection::clear()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _selection.name.clear();
        _selection.path.clear();
    }
}
