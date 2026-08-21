#pragma once

#include <cstddef>
#include <cstdint>

namespace D2B
{
    static const std::uint32_t kVoltageValid = 0x01;
    static const std::uint32_t kCurrentValid = 0x02;
    static const std::size_t kAcquisitionQueueDepth = 64;

    struct AcquisitionSample
    {
        std::uint64_t sequence;
        std::uint64_t timestampUs;
        std::uint32_t validMask;
        float voltageV;
        float currentA;
    };

    std::uint32_t BuildViValidMask(bool voltageReadSucceeded,
                                   float voltageV,
                                   bool currentReadSucceeded,
                                   float currentA,
                                   bool overflowReadSucceeded,
                                   bool overflow);

    AcquisitionSample BuildAcquisitionSample(std::uint64_t sequence,
                                              std::uint64_t timestampUs,
                                              std::uint32_t validMask,
                                              float voltageV,
                                              float currentA);

    class AcquisitionRing
    {
    public:
        AcquisitionRing();

        bool pushDropOldest(const AcquisitionSample& sample);
        bool pop(AcquisitionSample& sample, bool& producerOverflowBeforeSample);
        void clear();

        std::size_t queuedCount() const { return _count; }
        std::uint64_t producerDropCount() const { return _producerDropCount; }

    private:
        AcquisitionSample _samples[kAcquisitionQueueDepth];
        std::size_t _head;
        std::size_t _count;
        std::uint64_t _producerDropCount;
        bool _producerOverflowPending;
    };
} // namespace D2B
