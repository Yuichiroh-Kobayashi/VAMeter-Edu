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
    uint8_t viewer_asset_manifest[1364];
    uint8_t viewer_css_gzip[2385];
    uint8_t viewer_js_gzip[25809];
    uint8_t viewer_bundle_id[65];
};
