#include "meter_help_qr_view.h"

#include "../../../../../assets/assets.h"
#include "../../../../../hal/hal.h"
#include "../../../qrcode/qrcode.h"

#include <mooncake.h>

namespace SYSTEM
{
    namespace UI
    {
        MeterHelpQrView::MeterHelpQrView(METER_HELP_QR::MeterHelpKind kind)
            : _kind(kind),
              _isRenderable(METER_HELP_QR::BuildHelpQrBitmap(_qrBitmap, METER_HELP_QR::SelectHelpUrl(kind), _geometry))
        {
        }

        bool MeterHelpQrView::update(bool sideClicked, bool encoderClicked, std::uint32_t themeColor)
        {
            render(themeColor);
            return METER_HELP_QR::ResolveHelpInput(sideClicked, encoderClicked) == METER_HELP_QR::MeterHelpAction::Return;
        }

        METER_HELP_QR::MeterHelpKind MeterHelpQrView::kind() const { return _kind; }

        const METER_HELP_QR::HelpQrGeometry& MeterHelpQrView::geometry() const { return _geometry; }

        bool MeterHelpQrView::isRenderable() const { return _isRenderable; }

        void MeterHelpQrView::render(std::uint32_t themeColor)
        {
            LGFX_SpriteFx* canvas = HAL::GetCanvas();
            canvas->fillScreen(themeColor);
            canvas->setTextDatum(top_center);
            canvas->setTextColor(TFT_WHITE, themeColor);
            AssetPool::LoadFont24(canvas);
            canvas->drawString(_kind == METER_HELP_QR::MeterHelpKind::Voltage ? "Voltage Help" : "Current Help", 120, 2);

            if (_isRenderable)
            {
                static const int kAreaX = (240 - METER_HELP_QR::kHelpQrAreaPixels) / 2;
                static const int kAreaY = 28;
                const int centeredOffset = (METER_HELP_QR::kHelpQrAreaPixels - _geometry.renderPixels) / 2;
                const int quietPixels = METER_HELP_QR::kHelpQrQuietZoneModules * _geometry.moduleScale;
                const int innerPixels = _geometry.moduleCount * _geometry.moduleScale;

                canvas->fillRect(kAreaX, kAreaY, METER_HELP_QR::kHelpQrAreaPixels, METER_HELP_QR::kHelpQrAreaPixels, TFT_WHITE);
                QRCODE::RenderQRCodeBitmap(_qrBitmap,
                                           kAreaX + centeredOffset + quietPixels,
                                           kAreaY + centeredOffset + quietPixels,
                                           innerPixels,
                                           TFT_BLACK,
                                           TFT_WHITE);
            }
            else
            {
                AssetPool::LoadFont14(canvas);
                canvas->setTextDatum(middle_center);
                canvas->drawString("Help unavailable", 120, 120);
            }

            AssetPool::LoadFont14(canvas);
            canvas->setTextDatum(middle_center);
            canvas->setTextColor(TFT_WHITE, themeColor);
            canvas->drawString("Side: Back", 120, 228);
            HAL::CanvasUpdate();
        }
    } // namespace UI
} // namespace SYSTEM
