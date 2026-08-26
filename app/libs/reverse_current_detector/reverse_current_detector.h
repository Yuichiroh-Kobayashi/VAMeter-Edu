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
