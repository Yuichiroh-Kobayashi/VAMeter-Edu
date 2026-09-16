#include "live_share_view.h"

#include "../../../assets/assets.h"
#include "../../../hal/hal.h"
#include "../../../libs/live_share_qr_presentation/live_share_qr_presentation.h"
#include "../../utils/qrcode/qrcode.h"

namespace VIEWS
{
    namespace
    {
        // The frame sits outside the plate, the plate keyline is one pixel outside the
        // plate, and both stay clear of the QR matrix and its quiet area.
        const int kFrameThickness = 3;
        const int kFrameOutset = 5;
        const int kBracketLegPixels = 48;
        const int kBadgeCenterY = 19;
        const int kHintCenterY = 231;
        const int kCanvasCenterX = 120;

        void DrawFrame(LGFX_SpriteFx* canvas, LIVE_SHARE_QR_PRESENTATION::Frame frame, std::uint32_t color)
        {
            const int x = LIVE_SHARE_QR_PRESENTATION::kPlateX - kFrameOutset;
            const int y = LIVE_SHARE_QR_PRESENTATION::kPlateY - kFrameOutset;
            const int size = LIVE_SHARE_QR_PRESENTATION::kPlatePixels + 2 * kFrameOutset;
            const int thickness = kFrameThickness;

            if (frame == LIVE_SHARE_QR_PRESENTATION::Frame::SolidRect)
            {
                // Closed rectangle.
                canvas->fillRect(x, y, size, thickness, color);
                canvas->fillRect(x, y + size - thickness, size, thickness, color);
                canvas->fillRect(x, y, thickness, size, color);
                canvas->fillRect(x + size - thickness, y, thickness, size, color);
            }
            else if (frame == LIVE_SHARE_QR_PRESENTATION::Frame::CornerBrackets)
            {
                // Open frame: four corner brackets with a wide gap on every edge.
                const int leg = kBracketLegPixels;
                canvas->fillRect(x, y, leg, thickness, color);
                canvas->fillRect(x, y, thickness, leg, color);
                canvas->fillRect(x + size - leg, y, leg, thickness, color);
                canvas->fillRect(x + size - thickness, y, thickness, leg, color);
                canvas->fillRect(x, y + size - thickness, leg, thickness, color);
                canvas->fillRect(x, y + size - leg, thickness, leg, color);
                canvas->fillRect(x + size - leg, y + size - thickness, leg, thickness, color);
                canvas->fillRect(x + size - thickness, y + size - leg, thickness, leg, color);
            }
        }

        void RenderQrScreen(LGFX_SpriteFx* canvas,
                            const LIVE_SHARE_QR_PRESENTATION::Style& style,
                            std::vector<std::vector<bool>>& bitmap)
        {
            const int plateX = LIVE_SHARE_QR_PRESENTATION::kPlateX;
            const int plateY = LIVE_SHARE_QR_PRESENTATION::kPlateY;
            const int plateSize = LIVE_SHARE_QR_PRESENTATION::kPlatePixels;

            canvas->fillScreen(style.background);

            // Large state badge. Uses the font already present in the static asset pool.
            canvas->setTextSize(1);
            canvas->loadFont(AssetPool::GetStaticAsset()->Font.montserrat_semibold_36);
            canvas->setTextDatum(middle_center);
            canvas->setTextColor(style.foreground, style.background);
            canvas->drawString(style.badge, kCanvasCenterX, kBadgeCenterY);

            DrawFrame(canvas, style.frame, style.foreground);

            // Plate keyline then the white plate itself, so the plate edge stays readable
            // on a light background without shrinking the quiet area.
            canvas->fillRect(plateX - 1, plateY - 1, plateSize + 2, plateSize + 2, style.foreground);
            canvas->fillRect(plateX, plateY, plateSize, plateSize, TFT_WHITE);

            const LIVE_SHARE_QR_PRESENTATION::PlateGeometry geometry =
                LIVE_SHARE_QR_PRESENTATION::EvaluatePlateGeometry(static_cast<int>(bitmap.size()));
            if (geometry.valid)
            {
                QRCODE::RenderQRCodeBitmap(bitmap,
                                           plateX + geometry.qrOffsetInPlate,
                                           plateY + geometry.qrOffsetInPlate,
                                           geometry.qrPixels,
                                           TFT_BLACK,
                                           TFT_WHITE);
            }
            else
            {
                // Keep the state identifiable rather than drawing an out-of-plate matrix.
                AssetPool::LoadFont14(canvas);
                canvas->setTextDatum(middle_center);
                canvas->setTextColor(TFT_BLACK, TFT_WHITE);
                canvas->drawString("QR unavailable", kCanvasCenterX, plateY + plateSize / 2);
            }

            AssetPool::LoadFont14(canvas);
            canvas->setTextDatum(middle_center);
            canvas->setTextColor(style.foreground, style.background);
            canvas->drawString(style.hint, kCanvasCenterX, kHintCenterY);

            HAL::CanvasUpdate();
        }
    } // namespace

    LiveShareView::LiveShareView() : _preparedState(LIVE_SHARE_SESSION::State::Inactive) {}

    void LiveShareView::update(LIVE_SHARE_SESSION::State state,
                               std::uint32_t themeColor,
                               const std::string& activeApSsid,
                               const std::string& trustedViewerUrl,
                               LIVE_SHARE_SESSION::StartOutcome startOutcome)
    {
        prepareQr(state, activeApSsid, trustedViewerUrl);
        render(state, themeColor, startOutcome);
    }

    void LiveShareView::prepareQr(LIVE_SHARE_SESSION::State state,
                                  const std::string& activeApSsid,
                                  const std::string& trustedViewerUrl)
    {
        std::string payload;
        if (state == LIVE_SHARE_SESSION::State::WifiQr)
            payload = LIVE_SHARE_SESSION::BuildWifiQrPayload(activeApSsid);
        else if (state == LIVE_SHARE_SESSION::State::ViewerQr)
            payload = trustedViewerUrl;

        if (_preparedState == state && _preparedPayload == payload)
            return;
        _preparedState = state;
        _preparedPayload = payload;
        _qrBitmap.clear();
        if (!payload.empty())
            QRCODE::GetQrcodeBitmap(_qrBitmap, payload.c_str());
    }

    void LiveShareView::render(LIVE_SHARE_SESSION::State state,
                               std::uint32_t themeColor,
                               LIVE_SHARE_SESSION::StartOutcome startOutcome)
    {
        LGFX_SpriteFx* canvas = HAL::GetCanvas();

        const LIVE_SHARE_QR_PRESENTATION::Style style = LIVE_SHARE_QR_PRESENTATION::SelectStyle(state);
        if (style.valid)
        {
            RenderQrScreen(canvas, style, _qrBitmap);
            return;
        }

        canvas->fillScreen(themeColor);
        canvas->setTextDatum(top_center);
        canvas->setTextColor(TFT_WHITE, themeColor);
        AssetPool::LoadFont24(canvas);

        const char* title = "Live View";
        if (state == LIVE_SHARE_SESSION::State::Stopping)
            title = "Stopping Live View";
        else if (state == LIVE_SHARE_SESSION::State::StopRecovery)
        {
            if (startOutcome == LIVE_SHARE_SESSION::StartOutcome::RetainedApNeedsStopRetry)
                title = "Wi-Fi Cleanup";
            else if (startOutcome == LIVE_SHARE_SESSION::StartOutcome::RetainedServerNeedsStopRetry)
                title = "Server Cleanup";
            else
                title = "Stop Recovery";
        }
        else if (state == LIVE_SHARE_SESSION::State::StartError)
        {
            switch (startOutcome)
            {
            case LIVE_SHARE_SESSION::StartOutcome::BusyOtherOwner:
                title = "Live View Busy";
                break;
            case LIVE_SHARE_SESSION::StartOutcome::ApStartFailed:
                title = "Wi-Fi Start Error";
                break;
            case LIVE_SHARE_SESSION::StartOutcome::RouteOrRegistrationFailure:
                title = "Route Start Error";
                break;
            case LIVE_SHARE_SESSION::StartOutcome::AllocationOrListenFailure:
                title = "Server Start Error";
                break;
            default:
                title = "Live View Error";
                break;
            }
        }
        canvas->drawString(title, 120, 4);

        AssetPool::LoadFont14(canvas);
        canvas->setTextDatum(middle_center);
        canvas->setTextColor(TFT_WHITE, themeColor);
        if (state == LIVE_SHARE_SESSION::State::Starting)
        {
            canvas->drawString("Starting...", 120, 120);
        }
        else if (state == LIVE_SHARE_SESSION::State::Stopping)
        {
            canvas->drawString("Measurement continues", 120, 112);
            canvas->drawString("Closing network share...", 120, 132);
        }
        else if (state == LIVE_SHARE_SESSION::State::StopRecovery)
        {
            canvas->drawString("Encoder: Retry Stop", 120, 105);
            canvas->drawString("Power-cycle Help", 120, 130);
            canvas->drawString("Relay unchanged", 120, 155);
        }
        else if (state == LIVE_SHARE_SESSION::State::StartError)
        {
            canvas->drawString("Sharing did not start", 120, 110);
            canvas->drawString("Side: Dismiss", 120, 135);
        }
        HAL::CanvasUpdate();
    }
} // namespace VIEWS
