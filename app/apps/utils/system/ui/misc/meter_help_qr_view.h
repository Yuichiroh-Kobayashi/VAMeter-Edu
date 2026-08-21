#pragma once

#include "../../../../../libs/meter_help_qr/meter_help_qr_urls.h"

#include <cstdint>
#include <vector>

namespace SYSTEM
{
    namespace UI
    {
        class MeterHelpQrView
        {
        public:
            explicit MeterHelpQrView(METER_HELP_QR::MeterHelpKind kind);

            bool update(bool sideClicked, bool encoderClicked, std::uint32_t themeColor);
            METER_HELP_QR::MeterHelpKind kind() const;
            const METER_HELP_QR::HelpQrGeometry& geometry() const;
            bool isRenderable() const;

        private:
            void render(std::uint32_t themeColor);

            METER_HELP_QR::MeterHelpKind _kind;
            METER_HELP_QR::HelpQrGeometry _geometry;
            std::vector<std::vector<bool>> _qrBitmap;
            bool _isRenderable;
        };
    } // namespace UI
} // namespace SYSTEM
