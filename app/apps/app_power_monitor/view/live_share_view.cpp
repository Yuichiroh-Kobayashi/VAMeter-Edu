#include "live_share_view.h"

#include "../../../assets/assets.h"
#include "../../../hal/hal.h"
#include "../../utils/qrcode/qrcode.h"

namespace VIEWS
{
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
        canvas->fillScreen(themeColor);
        canvas->setTextDatum(top_center);
        canvas->setTextColor(TFT_WHITE, themeColor);
        AssetPool::LoadFont24(canvas);

        const char* title = "Live View";
        if (state == LIVE_SHARE_SESSION::State::Stopping)
            title = "Stopping Live View";
        else if (state == LIVE_SHARE_SESSION::State::StopRecovery)
            title = "Stop Recovery";
        else if (state == LIVE_SHARE_SESSION::State::StartError)
            title = startOutcome == LIVE_SHARE_SESSION::StartOutcome::Busy ? "Live View Busy" : "Live View Error";
        canvas->drawString(title, 120, 4);

        if ((state == LIVE_SHARE_SESSION::State::WifiQr || state == LIVE_SHARE_SESSION::State::ViewerQr) &&
            !_qrBitmap.empty())
        {
            const int qrSize = 150;
            QRCODE::RenderQRCodeBitmap(_qrBitmap, 120 - qrSize / 2, 34, qrSize, TFT_BLACK, TFT_WHITE);
            AssetPool::LoadFont14(canvas);
            canvas->setTextDatum(middle_center);
            canvas->setTextColor(TFT_WHITE, themeColor);
            canvas->drawString(state == LIVE_SHARE_SESSION::State::WifiQr ? "Join device Wi-Fi" : "Open Viewer",
                               120,
                               194);
            canvas->drawString(state == LIVE_SHARE_SESSION::State::WifiQr ? "Encoder: Next" : "Encoder: Wi-Fi QR",
                               120,
                               211);
            canvas->drawString("Side: Stop", 120, 227);
        }
        else
        {
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
                canvas->drawString("Side: Retry", 120, 135);
            }
        }
        HAL::CanvasUpdate();
    }
} // namespace VIEWS
