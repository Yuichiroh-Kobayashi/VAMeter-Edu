#include "live_share_qr_presentation.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace P = LIVE_SHARE_QR_PRESENTATION;
namespace LSS = LIVE_SHARE_SESSION;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    // Integer approximation of relative luminance, sufficient to assert light/dark polarity.
    unsigned Luma(std::uint32_t rgb)
    {
        const unsigned r = (rgb >> 16) & 0xFFU;
        const unsigned g = (rgb >> 8) & 0xFFU;
        const unsigned b = rgb & 0xFFU;
        return (2126U * r + 7152U * g + 722U * b) / 10000U;
    }

    // Amendment 27-A: red/orange/amber/yellow stay reserved for warning and fault
    // semantics, so a normal QR state background must not be red-dominant.
    bool IsAlarmFamily(std::uint32_t rgb)
    {
        const unsigned r = (rgb >> 16) & 0xFFU;
        const unsigned g = (rgb >> 8) & 0xFFU;
        const unsigned b = rgb & 0xFFU;
        return r > b && r >= g;
    }

    const LSS::State kNonQrStates[] = {
        LSS::State::Inactive,
        LSS::State::Starting,
        LSS::State::Stopping,
        LSS::State::StopRecovery,
        LSS::State::StartError,
    };
    const std::size_t kNonQrStateCount = sizeof(kNonQrStates) / sizeof(kNonQrStates[0]);

    void TestQrStyleIdentity()
    {
        const P::Style wifi = P::SelectStyle(LSS::State::WifiQr);
        const P::Style viewer = P::SelectStyle(LSS::State::ViewerQr);

        Require(wifi.valid, "T01 Wi-Fi QR state has a QR presentation style");
        Require(viewer.valid, "T02 Viewer QR state has a QR presentation style");

        Require(wifi.background != viewer.background, "T03 QR state backgrounds differ");
        Require(wifi.foreground != viewer.foreground, "T04 QR state foregrounds differ");

        Require(wifi.badge != nullptr && viewer.badge != nullptr, "T05 both QR states carry a badge");
        Require(std::strcmp(wifi.badge, viewer.badge) != 0, "T05 QR state badges differ");
        Require(std::strlen(wifi.badge) > 0U && std::strlen(viewer.badge) > 0U, "T05 badges are non-empty");

        Require(wifi.hint != nullptr && viewer.hint != nullptr, "T06 both QR states carry a hint");
        Require(std::strcmp(wifi.hint, viewer.hint) != 0, "T06 QR state hints differ");

        Require(wifi.frame != P::Frame::None, "T07 Wi-Fi QR draws a frame");
        Require(viewer.frame != P::Frame::None, "T07 Viewer QR draws a frame");
        Require(wifi.frame != viewer.frame, "T07 QR state frame geometry differs");

        // Text polarity is an independent cue from hue.
        Require(Luma(wifi.background) < Luma(wifi.foreground), "T08 Wi-Fi QR is light text on dark background");
        Require(Luma(viewer.background) > Luma(viewer.foreground), "T08 Viewer QR is dark text on light background");
        Require(Luma(wifi.background) < Luma(viewer.background), "T08 QR state backgrounds have opposite polarity");

        // A viewer that cannot resolve hue must still separate the two screens.
        const unsigned lumaGap = Luma(viewer.background) - Luma(wifi.background);
        Require(lumaGap >= 96U, "T09 QR backgrounds keep a large luminance separation");

        Require(!IsAlarmFamily(wifi.background), "T10 Wi-Fi background avoids reserved alarm colors");
        Require(!IsAlarmFamily(viewer.background), "T10 Viewer background avoids reserved alarm colors");

        std::cout << "T01-T10 wifi_bg=0x" << std::hex << wifi.background << " viewer_bg=0x" << viewer.background << std::dec
                  << " luma_gap=" << lumaGap << " PASS\n";
    }

    void TestSelectorIsDeterministic()
    {
        const P::Style wifi = P::SelectStyle(LSS::State::WifiQr);
        const P::Style viewer = P::SelectStyle(LSS::State::ViewerQr);
        for (int iteration = 0; iteration < 100; ++iteration)
        {
            const P::Style repeatedWifi = P::SelectStyle(LSS::State::WifiQr);
            const P::Style repeatedViewer = P::SelectStyle(LSS::State::ViewerQr);
            Require(repeatedWifi.valid == wifi.valid && repeatedWifi.background == wifi.background &&
                        repeatedWifi.foreground == wifi.foreground && repeatedWifi.frame == wifi.frame &&
                        repeatedWifi.badge == wifi.badge && repeatedWifi.hint == wifi.hint,
                    "T11 Wi-Fi style selection is deterministic");
            Require(repeatedViewer.valid == viewer.valid && repeatedViewer.background == viewer.background &&
                        repeatedViewer.foreground == viewer.foreground && repeatedViewer.frame == viewer.frame &&
                        repeatedViewer.badge == viewer.badge && repeatedViewer.hint == viewer.hint,
                    "T11 Viewer style selection is deterministic");
        }
        std::cout << "T11 selector determinism PASS\n";
    }

    void TestNonQrStatesInheritNothing()
    {
        const P::Style wifi = P::SelectStyle(LSS::State::WifiQr);
        const P::Style viewer = P::SelectStyle(LSS::State::ViewerQr);

        const P::Style neutral;
        for (std::size_t index = 0; index < kNonQrStateCount; ++index)
        {
            const P::Style style = P::SelectStyle(kNonQrStates[index]);
            Require(!style.valid, "T12 non-QR state has no QR presentation style");
            Require(style.frame == P::Frame::None, "T12 non-QR state draws no frame");
            Require(style.badge == nullptr, "T12 non-QR state has no badge");
            Require(style.hint == nullptr, "T12 non-QR state has no hint");
            Require(style.background == neutral.background && style.foreground == neutral.foreground,
                    "T12 non-QR state returns the neutral default style");
            Require(style.background != wifi.background && style.background != viewer.background,
                    "T13 non-QR state does not inherit a QR background");
        }

        // Values outside the declared enumeration must fail closed, not fall through to a
        // QR style.
        const P::Style outOfRange = P::SelectStyle(static_cast<LSS::State>(200));
        Require(!outOfRange.valid, "T14 unknown state value fails closed");
        Require(outOfRange.frame == P::Frame::None && outOfRange.badge == nullptr, "T14 unknown state draws nothing");

        std::cout << "T12-T14 non-QR states PASS\n";
    }

    void RequirePlateInvariants(const P::PlateGeometry& geometry, const char* label)
    {
        Require(geometry.moduleScale > 0, label);
        Require(geometry.qrPixels > 0 && geometry.qrPixels <= P::kPlatePixels, label);
        Require(geometry.qrPixels == geometry.moduleCount * geometry.moduleScale, label);
        Require(geometry.quietPixelsMinimum == P::kQuietZoneModules * geometry.moduleScale, label);
        Require(geometry.qrOffsetInPlate >= geometry.quietPixelsMinimum, label);
        const int trailing = P::kPlatePixels - geometry.qrPixels - geometry.qrOffsetInPlate;
        Require(trailing >= geometry.quietPixelsMinimum, label);
        // The renderer receives qrPixels as its size argument and derives the same integer
        // module scale from it.
        Require(geometry.qrPixels / geometry.moduleCount == geometry.moduleScale, label);
    }

    void TestRepresentativeGeometry()
    {
        // Regression fixtures for the payloads currently produced on device. They are not
        // a contract that future payloads must keep these module counts.
        const P::PlateGeometry twentyFive = P::EvaluatePlateGeometry(25);
        Require(twentyFive.valid, "G01 25-module matrix is supported");
        Require(twentyFive.moduleScale == 6, "G01 25-module matrix keeps module scale 6");
        Require(twentyFive.qrPixels == 150, "G01 25-module matrix renders 150 px");
        Require(twentyFive.quietPixelsMinimum == 12, "G01 25-module quiet minimum is 12 px");
        Require(twentyFive.qrOffsetInPlate == 12, "G01 25-module matrix is centered at offset 12");
        RequirePlateInvariants(twentyFive, "G01 25-module plate invariants");

        const P::PlateGeometry twentyNine = P::EvaluatePlateGeometry(29);
        Require(twentyNine.valid, "G02 29-module matrix is supported");
        Require(twentyNine.moduleScale == 5, "G02 29-module matrix keeps module scale 5");
        Require(twentyNine.qrPixels == 145, "G02 29-module matrix renders 145 px");
        Require(twentyNine.quietPixelsMinimum == 10, "G02 29-module quiet minimum is 10 px");
        Require(twentyNine.qrOffsetInPlate == 14, "G02 29-module matrix is centered at offset 14");
        RequirePlateInvariants(twentyNine, "G02 29-module plate invariants");

        // The previous screen drew these matrices at scale 6 and 5 respectively. The plate
        // must not shrink them.
        Require(twentyFive.moduleScale >= 6, "G03 25-module scale is not reduced");
        Require(twentyNine.moduleScale >= 5, "G03 29-module scale is not reduced");

        std::cout << "G01-G03 plate=" << P::kPlatePixels << " quiet_modules=" << P::kQuietZoneModules
                  << " m25_scale=" << twentyFive.moduleScale << " m29_scale=" << twentyNine.moduleScale << " PASS\n";
    }

    void TestSupportedGeometryRange()
    {
        int validCount = 0;
        int largestValid = 0;
        for (int moduleCount = P::kMinimumModuleCount; moduleCount <= P::kMaximumModuleCount; ++moduleCount)
        {
            const P::PlateGeometry geometry = P::EvaluatePlateGeometry(moduleCount);
            Require(geometry.moduleCount == moduleCount, "G04 geometry echoes the requested module count");
            if (!geometry.valid)
                continue;
            RequirePlateInvariants(geometry, "G04 plate invariants hold across the supported range");
            ++validCount;
            largestValid = moduleCount;
        }
        Require(validCount > 1, "G04 more than the two representative counts are supported");
        Require(P::EvaluatePlateGeometry(P::kMinimumModuleCount).valid, "G05 smallest QR matrix is supported");
        Require(P::EvaluatePlateGeometry(largestValid + 1).valid == false,
                "G05 the first unsupported count above the range fails closed");

        std::cout << "G04-G05 supported_counts=" << validCount << " largest_valid=" << largestValid << " PASS\n";
    }

    void TestInvalidGeometryFailsClosed()
    {
        const int rejected[] = {0, -1, -29, -1000, 1, 20, P::kMaximumModuleCount + 1, 1000};
        const std::size_t rejectedCount = sizeof(rejected) / sizeof(rejected[0]);
        for (std::size_t index = 0; index < rejectedCount; ++index)
        {
            const P::PlateGeometry geometry = P::EvaluatePlateGeometry(rejected[index]);
            Require(!geometry.valid, "G06 unsupported module count fails closed");
            Require(geometry.moduleScale == 0 && geometry.qrPixels == 0, "G06 invalid geometry carries no draw size");
            Require(geometry.qrOffsetInPlate == 0 && geometry.quietPixelsMinimum == 0,
                    "G06 invalid geometry carries no offset");
        }

        // A matrix too large to leave any quiet area inside the plate must be rejected even
        // though it is a legal QR module count.
        const P::PlateGeometry oversized = P::EvaluatePlateGeometry(P::kMaximumModuleCount);
        Require(!oversized.valid, "G07 a matrix with no usable module scale fails closed");

        std::cout << "G06-G07 invalid geometry PASS\n";
    }

    void TestPlateFitsCanvas()
    {
        Require(P::kPlateX >= 0 && P::kPlateY >= 0, "G08 plate origin is on canvas");
        Require(P::kPlateX + P::kPlatePixels <= P::kScreenPixels, "G08 plate fits the canvas width");
        Require(P::kPlateY + P::kPlatePixels <= P::kScreenPixels, "G08 plate fits the canvas height");
        Require(P::kQuietZoneModules > 0, "G08 a quiet area is configured");
        std::cout << "G08 plate origin=(" << P::kPlateX << "," << P::kPlateY << ") size=" << P::kPlatePixels
                  << " canvas=" << P::kScreenPixels << " PASS\n";
    }
} // namespace

int main()
{
    static_assert(sizeof(P::Style) <= 64U, "presentation style stays a small value type");
    static_assert(sizeof(P::PlateGeometry) <= 64U, "plate geometry stays a small value type");

    TestQrStyleIdentity();
    TestSelectorIsDeterministic();
    TestNonQrStatesInheritNothing();
    TestRepresentativeGeometry();
    TestSupportedGeometryRange();
    TestInvalidGeometryFailsClosed();
    TestPlateFitsCanvas();

    std::cout << "live_share_qr_presentation_test PASS\n";
    return 0;
}
