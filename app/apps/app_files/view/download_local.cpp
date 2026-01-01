/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../app_files.h"
#include "../../utils/system/system.h"
#include "../../utils/system/ui/misc/misc.h"
#include <string>

using namespace MOONCAKE::APPS;

void AppFiles::_on_page_download_local(const std::string& recordName) { SYSTEM::UI::CreateDownloadQRPage(recordName); }
