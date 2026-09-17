#pragma once

#include <cstdint>

namespace REVERSE_CURRENT_SAFETY_DEVICE
{
    bool IsTrainingArmed();
    bool IsRelayPolicyInitialized();
    void Observe(bool currentValid, float processedSignedCurrentA, std::uint8_t currentRange);
    void CommitFaultAndOpenRelay();
} // namespace REVERSE_CURRENT_SAFETY_DEVICE
