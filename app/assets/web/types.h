/*
* SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
*
* SPDX-License-Identifier: MIT
*/
#pragma once
#include <cstdint>

/* -------------------------------------------------------------------------- */
/*                                  Web pages                                 */
/* -------------------------------------------------------------------------- */
struct WebPagePool_t
{
    uint8_t syscfg[80120];
    uint8_t favicon[5182];
    uint8_t viewer_index_html[573];
    uint8_t viewer_asset_manifest[1363];
    uint8_t viewer_css_gzip[580];
    uint8_t viewer_js_gzip[18790];
    uint8_t viewer_bundle_id[65];
};
