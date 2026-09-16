#include "live_share_qr_presentation.h"

namespace LIVE_SHARE_QR_PRESENTATION
{
    Style SelectStyle(LIVE_SHARE_SESSION::State state)
    {
        Style style;
        switch (state)
        {
        case LIVE_SHARE_SESSION::State::WifiQr:
            style.valid = true;
            style.background = kWifiBackground;
            style.foreground = kWifiForeground;
            style.frame = Frame::SolidRect;
            style.badge = "WI-FI";
            style.hint = "Encoder: Viewer QR   Side: Stop";
            break;
        case LIVE_SHARE_SESSION::State::ViewerQr:
            style.valid = true;
            style.background = kViewerBackground;
            style.foreground = kViewerForeground;
            style.frame = Frame::CornerBrackets;
            style.badge = "VIEWER";
            style.hint = "Encoder: Wi-Fi QR   Side: Stop";
            break;
        case LIVE_SHARE_SESSION::State::Inactive:
        case LIVE_SHARE_SESSION::State::Starting:
        case LIVE_SHARE_SESSION::State::Stopping:
        case LIVE_SHARE_SESSION::State::StopRecovery:
        case LIVE_SHARE_SESSION::State::StartError:
        default:
            // Non-QR states keep the existing theme-color rendering path. They never
            // inherit a QR background, frame or badge.
            break;
        }
        return style;
    }

    PlateGeometry EvaluatePlateGeometry(int moduleCount)
    {
        PlateGeometry geometry;
        geometry.moduleCount = moduleCount;

        if (moduleCount < kMinimumModuleCount || moduleCount > kMaximumModuleCount)
            return geometry;

        const int spanModules = moduleCount + 2 * kQuietZoneModules;
        const int moduleScale = kPlatePixels / spanModules;
        if (moduleScale <= 0)
            return geometry;

        const int qrPixels = moduleCount * moduleScale;
        const int quietPixelsMinimum = kQuietZoneModules * moduleScale;
        const int qrOffsetInPlate = (kPlatePixels - qrPixels) / 2;

        // Fail closed instead of handing the renderer a rectangle that leaves the plate or
        // eats into the candidate quiet area on either side.
        if (qrPixels <= 0 || qrPixels > kPlatePixels)
            return geometry;
        if (qrOffsetInPlate < quietPixelsMinimum)
            return geometry;
        if (kPlatePixels - qrPixels - qrOffsetInPlate < quietPixelsMinimum)
            return geometry;

        geometry.valid = true;
        geometry.moduleScale = moduleScale;
        geometry.qrPixels = qrPixels;
        geometry.quietPixelsMinimum = quietPixelsMinimum;
        geometry.qrOffsetInPlate = qrOffsetInPlate;
        return geometry;
    }
} // namespace LIVE_SHARE_QR_PRESENTATION
