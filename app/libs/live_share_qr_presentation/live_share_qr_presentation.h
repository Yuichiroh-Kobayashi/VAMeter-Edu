#pragma once

#include "../live_share_session/live_share_session.h"

#include <cstdint>

namespace LIVE_SHARE_QR_PRESENTATION
{
    // Canvas and plate geometry. The QR matrix is always drawn inside the white plate and
    // every distinguishing cue is drawn outside it, so no decoration touches a module.
    static const int kScreenPixels = 240;
    static const int kPlatePixels = 174;
    static const int kPlateX = 33;
    static const int kPlateY = 44;

    // Candidate quiet area kept around the matrix inside the plate. This improves on the
    // current Live View surrounding area; it is not a claim of full 4-module compliance.
    static const int kQuietZoneModules = 2;

    // Module-count range a QR symbol can take. Counts outside it fail closed.
    static const int kMinimumModuleCount = 21;
    static const int kMaximumModuleCount = 177;

    // Implementation candidate colors, not physically qualified display colors.
    // Red/orange/amber/yellow are deliberately unused: those families stay reserved for
    // warning and fault semantics.
    static const std::uint32_t kWifiBackground = 0x14213D;
    static const std::uint32_t kWifiForeground = 0xFFFFFF;
    static const std::uint32_t kViewerBackground = 0xA7E9F2;
    static const std::uint32_t kViewerForeground = 0x000000;

    enum class Frame : std::uint8_t
    {
        None = 0,
        SolidRect,
        CornerBrackets,
    };

    // Presentation for one Live Share state. Fields other than `valid` are meaningful only
    // when `valid` is true; non-QR states keep their existing theme-color rendering path.
    struct Style
    {
        bool valid = false;
        std::uint32_t background = 0U;
        std::uint32_t foreground = 0U;
        Frame frame = Frame::None;
        const char* badge = nullptr;
        const char* hint = nullptr;
    };

    struct PlateGeometry
    {
        bool valid = false;
        int moduleCount = 0;
        int moduleScale = 0;
        int qrPixels = 0;
        int quietPixelsMinimum = 0;
        int qrOffsetInPlate = 0;
    };

    // Deterministic, allocation-free mapping from session state to presentation.
    Style SelectStyle(LIVE_SHARE_SESSION::State state);

    // Integer plate geometry for a QR matrix of `moduleCount` modules per side.
    // Returns an invalid geometry rather than coordinates the renderer would draw
    // outside the plate.
    PlateGeometry EvaluatePlateGeometry(int moduleCount);
} // namespace LIVE_SHARE_QR_PRESENTATION
