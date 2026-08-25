#pragma once

#include "libs/f15_observation/f15_observation.h"

#include <cstdint>

namespace F15_OBS_DEVICE
{
    static const std::size_t kQueueCapacity = 32;
    static const std::uint32_t kDrainTaskStackBytes = 3072;

    bool Start();
    void Publish(std::uint64_t timestampUs,
                 float shuntCurrentA,
                 float busVoltageV,
                 std::uint32_t validMask,
                 F15_OBS::CurrentRange currentRange,
                 bool currentReadSucceeded,
                 bool overflowReadSucceeded,
                 bool overflowAsserted);
} // namespace F15_OBS_DEVICE
