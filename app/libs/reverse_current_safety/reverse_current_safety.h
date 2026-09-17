#pragma once

#include "libs/reverse_current_detector/reverse_current_detector.h"

#include <cstdint>

namespace REVERSE_CURRENT_SAFETY
{
    enum class CurrentRange : std::uint8_t
    {
        Low = 0,
        High = 1,
    };

    struct Action
    {
        bool requestRelayOff;
    };

    class Controller
    {
    public:
        explicit Controller(const REVERSE_CURRENT_DETECTOR::Configuration& configuration);
        Action observe(bool relayPolicyInitialized,
                       bool trainingArmed,
                       bool currentValid,
                       float processedSignedCurrentA,
                       CurrentRange range);

    private:
        REVERSE_CURRENT_DETECTOR::Detector _detector;
    };
} // namespace REVERSE_CURRENT_SAFETY
