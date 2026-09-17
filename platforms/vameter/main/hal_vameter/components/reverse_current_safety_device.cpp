#include "reverse_current_safety_device.h"

#include "libs/reverse_current_detector/reverse_current_detector.h"
#include "libs/reverse_current_safety/reverse_current_safety.h"

namespace
{
    REVERSE_CURRENT_SAFETY::Controller controller(REVERSE_CURRENT_DETECTOR::ProductionDisabledConfiguration());
}

namespace REVERSE_CURRENT_SAFETY_DEVICE
{
    bool IsTrainingArmed() { return false; }

    void Observe(bool currentValid, float processedSignedCurrentA, std::uint8_t currentRange)
    {
        const REVERSE_CURRENT_SAFETY::CurrentRange range =
            currentRange == 0 ? REVERSE_CURRENT_SAFETY::CurrentRange::Low : REVERSE_CURRENT_SAFETY::CurrentRange::High;
        const REVERSE_CURRENT_SAFETY::Action action =
            controller.observe(IsRelayPolicyInitialized(), IsTrainingArmed(), currentValid, processedSignedCurrentA, range);
        if (action.requestRelayOff)
            CommitFaultAndOpenRelay();
    }
} // namespace REVERSE_CURRENT_SAFETY_DEVICE
