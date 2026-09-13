#pragma once

#include "fonts/types.h"
#include "images/types.h"
#include "theme/types.h"
#include "localization/types.h"
#include "web/types.h"

/**
 * @brief A struct to define static binary asset
 *
 */
struct StaticAsset_t
{
    FontPool_t Font;
    ImagePool_t Image;
    ColorPool_t Color;
    TextPool_t Text;
    WebPagePool_t WebPage;
};
