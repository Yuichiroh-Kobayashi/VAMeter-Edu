#include "meter_help_qr_urls.h"

#include "../../apps/utils/qrcode/qrcodegen/qrcodegen.hpp"

#include <cstring>

namespace METER_HELP_QR
{
    const char VOLTAGE_METER_HELP_URL[] = "https://digi-keirin.com/js25/jrika/25jrika2/25jrika2a_22201_m_02.php";
    const char CURRENT_METER_HELP_URL[] = "https://digi-keirin.com/js25/jrika/25jrika2/25jrika2a_21401_m_02.php";

    namespace
    {
        HelpQrGeometry GeometryForModuleCount(int moduleCount)
        {
            HelpQrGeometry geometry;
            geometry.moduleCount = moduleCount;
            if (moduleCount < 21 || moduleCount > 177)
                return geometry;
            geometry.moduleScale = kHelpQrAreaPixels / (moduleCount + 2 * kHelpQrQuietZoneModules);
            geometry.renderPixels = (moduleCount + 2 * kHelpQrQuietZoneModules) * geometry.moduleScale;
            geometry.valid = geometry.moduleScale >= 2 && geometry.renderPixels <= kHelpQrAreaPixels;
            return geometry;
        }

        bool HasAbsoluteHttpAuthority(const char* url, std::size_t length)
        {
            std::size_t authorityOffset = 0;
            if (length > 7U && std::strncmp(url, "http://", 7U) == 0)
                authorityOffset = 7U;
            else if (length > 8U && std::strncmp(url, "https://", 8U) == 0)
                authorityOffset = 8U;
            else
                return false;

            if (authorityOffset >= length || url[authorityOffset] == '/' || url[authorityOffset] == '?' ||
                url[authorityOffset] == '#')
                return false;
            return true;
        }
    } // namespace

    const char* SelectHelpUrl(MeterHelpKind kind)
    {
        return kind == MeterHelpKind::Voltage ? VOLTAGE_METER_HELP_URL : CURRENT_METER_HELP_URL;
    }

    bool ValidateHelpUrl(const char* url)
    {
        if (url == nullptr)
            return false;
        const std::size_t length = std::strlen(url);
        if (length == 0U || length > kQrEncodeGuaranteedMaxBytes || !HasAbsoluteHttpAuthority(url, length))
            return false;
        for (std::size_t index = 0; index < length; ++index)
        {
            const unsigned char value = static_cast<unsigned char>(url[index]);
            if (value <= 0x20U || value == 0x7fU)
                return false;
        }
        return true;
    }

    HelpQrGeometry EvaluateHelpQrGeometry(const char* url)
    {
        if (!ValidateHelpUrl(url))
            return HelpQrGeometry();
        const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(url, qrcodegen::QrCode::Ecc::LOW);
        return GeometryForModuleCount(qr.getSize());
    }

    bool BuildHelpQrBitmap(std::vector<std::vector<bool>>& bitmap, const char* url, HelpQrGeometry& geometry)
    {
        bitmap.clear();
        geometry = HelpQrGeometry();
        if (!ValidateHelpUrl(url))
            return false;
        const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(url, qrcodegen::QrCode::Ecc::LOW);
        geometry = GeometryForModuleCount(qr.getSize());
        if (!geometry.valid)
            return false;
        bitmap.resize(static_cast<std::size_t>(geometry.moduleCount));
        for (int y = 0; y < geometry.moduleCount; ++y)
        {
            bitmap[static_cast<std::size_t>(y)].resize(static_cast<std::size_t>(geometry.moduleCount));
            for (int x = 0; x < geometry.moduleCount; ++x)
                bitmap[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = qr.getModule(x, y);
        }
        return true;
    }

    MeterHelpAction ResolveHelpInput(bool sideClicked, bool encoderClicked)
    {
        (void)encoderClicked;
        return sideClicked ? MeterHelpAction::Return : MeterHelpAction::None;
    }

    void MeterHelpInteraction::requestReturn() { _returnPending = true; }

    bool MeterHelpInteraction::returnPending() const { return _returnPending; }

    bool MeterHelpInteraction::canReleaseOwnership(bool sidePressed, bool encoderPressed) const
    {
        return _returnPending && !sidePressed && !encoderPressed;
    }

    void MeterHelpInteraction::reset() { _returnPending = false; }
} // namespace METER_HELP_QR
