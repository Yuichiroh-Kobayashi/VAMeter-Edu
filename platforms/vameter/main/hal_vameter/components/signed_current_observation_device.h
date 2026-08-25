#pragma once

#include "libs/signed_current_observation/signed_current_observation.h"

#include <cstdint>

namespace SIGNED_CURRENT_OBS_DEVICE
{
    static const std::size_t kQueueCapacity = 32;
    static const std::uint32_t kDrainTaskStackBytes = 3072;

    bool Start();
    void Publish(std::uint64_t timestampUs,
                 float shuntCurrentA,
                 float busVoltageV,
                 std::uint32_t validMask,
                 SIGNED_CURRENT_OBS::CurrentRange currentRange,
                 bool currentReadSucceeded,
                 bool overflowReadSucceeded,
                 bool overflowAsserted);
} // namespace SIGNED_CURRENT_OBS_DEVICE
