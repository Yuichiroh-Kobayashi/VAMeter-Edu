#include "reverse_current_safety.h"

namespace REVERSE_CURRENT_SAFETY
{
    Controller::Controller(const REVERSE_CURRENT_DETECTOR::Configuration& configuration)
        : _detector(configuration), _faultLatched(false)
    {
    }

    Action Controller::observe(
        bool relayPolicyInitialized, bool trainingArmed, bool currentValid, float processedSignedCurrentA, CurrentRange range)
    {
        (void)range; // Gate B0 preserves range switching with no special policy.
        if (_faultLatched || !relayPolicyInitialized || !trainingArmed)
            return {_faultLatched, false};

        const REVERSE_CURRENT_DETECTOR::Result result = _detector.observe(currentValid, processedSignedCurrentA);
        if (!result.enteredLatched)
            return {false, false};

        // Commit the pure policy latch before exposing the one-shot OFF request.
        _faultLatched = true;
        return {true, true};
    }

    bool Controller::isFaultLatched() const { return _faultLatched; }

    bool Controller::authorizeRelayOn() const { return !_faultLatched; }

    bool Controller::authorizeRelayOff() const { return true; }
} // namespace REVERSE_CURRENT_SAFETY
