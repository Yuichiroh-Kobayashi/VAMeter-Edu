#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace METER_HELP_QR
{
    static const int kHelpQrAreaPixels = 185;
    static const int kHelpQrQuietZoneModules = 4;
    static const std::size_t kQrEncodeGuaranteedMaxBytes = 2953U;

    enum class MeterHelpKind : std::uint8_t
    {
        Voltage,
        Current,
    };

    enum class MeterHelpAction : std::uint8_t
    {
        None,
        Return,
    };

    struct HelpQrGeometry
    {
        bool valid = false;
        int moduleCount = 0;
        int moduleScale = 0;
        int renderPixels = 0;
    };

    class MeterHelpInteraction
    {
    public:
        void requestReturn();
        bool returnPending() const;
        bool canReleaseOwnership(bool sidePressed, bool encoderPressed) const;
        void reset();

    private:
        bool _returnPending = false;
    };

    extern const char VOLTAGE_METER_HELP_URL[];
    extern const char CURRENT_METER_HELP_URL[];

    const char* SelectHelpUrl(MeterHelpKind kind);
    bool ValidateHelpUrl(const char* url);
    HelpQrGeometry EvaluateHelpQrGeometry(const char* url);
    bool BuildHelpQrBitmap(std::vector<std::vector<bool>>& bitmap, const char* url, HelpQrGeometry& geometry);
    MeterHelpAction ResolveHelpInput(bool sideClicked, bool encoderClicked);
} // namespace METER_HELP_QR
