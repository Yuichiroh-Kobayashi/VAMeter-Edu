#include "reverse_current_safety.h"

namespace REVERSE_CURRENT_SAFETY
{
    Controller::Controller(const REVERSE_CURRENT_DETECTOR::Configuration& configuration) : _detector(configuration) {}

    Action Controller::observe(
        bool relayPolicyInitialized, bool trainingArmed, bool currentValid, float processedSignedCurrentA, CurrentRange range)
    {
        (void)range; // Context only: B0 establishes no range-switch policy.
        // This is the Training protection detector. Non-Training warning
        // classification is separate future presentation work.
        if (!relayPolicyInitialized || !trainingArmed)
            return {false};

        const REVERSE_CURRENT_DETECTOR::Result result = _detector.observe(currentValid, processedSignedCurrentA);
        if (!result.enteredLatched)
            return {false};
        return {true};
    }
} // namespace REVERSE_CURRENT_SAFETY
