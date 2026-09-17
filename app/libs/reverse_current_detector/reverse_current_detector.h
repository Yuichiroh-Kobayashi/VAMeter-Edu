#pragma once

#include <cstdint>

namespace REVERSE_CURRENT_DETECTOR
{
    enum class State : std::uint8_t
    {
        Normal = 0,
        Candidate = 1,
        Latched = 2,
    };

    struct Configuration
    {
        float negativeThresholdA;
        std::uint32_t requiredQualifyingCount;
    };

    // Gate B0 deliberately has no production threshold.  This named factory
    // keeps the production construction site from looking like threshold
    // authority; the returned configuration is invalid and therefore inert.
    Configuration ProductionDisabledConfiguration();

    struct Result
    {
        State state;
        std::uint32_t qualifyingCount;
        bool enteredLatched;
    };

    class Detector
    {
    public:
        explicit Detector(const Configuration& configuration);
        bool isConfigurationValid() const;
        Result observe(bool measurementValid, float signedCurrentA);
        Result result() const;

    private:
        Configuration _configuration;
        bool _configurationValid;
        State _state;
        std::uint32_t _qualifyingCount;
    };
} // namespace REVERSE_CURRENT_DETECTOR
