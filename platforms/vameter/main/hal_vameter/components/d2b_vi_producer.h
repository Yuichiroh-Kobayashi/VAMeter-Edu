#pragma once

#include "libs/d2b_vi/d2b_acquisition.h"

#include <cstdint>

namespace D2B_PRODUCER
{
    struct Snapshot
    {
        bool active;
        bool hasSample;
        bool sequenceExhausted;
        std::uint32_t streamId;
        std::uint64_t nextSequence;
        std::uint64_t lastTimestampUs;
        std::uint64_t producerDropCount;
        std::uint32_t queuedSampleCount;
    };

    void Start(std::uint32_t streamId);
    void Abort(std::uint32_t streamId);
    void Tap(std::uint64_t timestampUs, std::uint32_t validMask, float voltageV, float currentA);
    bool Pop(D2B::AcquisitionSample& sample, bool& producerOverflowBeforeSample, std::uint32_t& streamId);
    Snapshot GetSnapshot();
} // namespace D2B_PRODUCER
