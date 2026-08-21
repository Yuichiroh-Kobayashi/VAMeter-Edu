#include "d2b_acquisition.h"

#include <cmath>
#include <limits>

namespace D2B
{
    std::uint32_t BuildViValidMask(bool voltageReadSucceeded,
                                   float voltageV,
                                   bool currentReadSucceeded,
                                   float currentA,
                                   bool overflowReadSucceeded,
                                   bool overflow)
    {
        std::uint32_t result = 0;
        if (voltageReadSucceeded && std::isfinite(voltageV))
            result |= kVoltageValid;
        if (currentReadSucceeded && std::isfinite(currentA) && overflowReadSucceeded && !overflow)
            result |= kCurrentValid;
        return result;
    }

    AcquisitionSample BuildAcquisitionSample(std::uint64_t sequence,
                                              std::uint64_t timestampUs,
                                              std::uint32_t validMask,
                                              float voltageV,
                                              float currentA)
    {
        std::uint32_t canonicalMask = validMask & (kVoltageValid | kCurrentValid);
        if ((canonicalMask & kVoltageValid) != 0 && !std::isfinite(voltageV))
            canonicalMask &= ~kVoltageValid;
        if ((canonicalMask & kCurrentValid) != 0 && !std::isfinite(currentA))
            canonicalMask &= ~kCurrentValid;

        const AcquisitionSample sample = {
            sequence,
            timestampUs,
            canonicalMask,
            (canonicalMask & kVoltageValid) != 0 ? voltageV : 0.0F,
            (canonicalMask & kCurrentValid) != 0 ? currentA : 0.0F,
        };
        return sample;
    }

    AcquisitionRing::AcquisitionRing()
        : _head(0), _count(0), _producerDropCount(0), _producerOverflowPending(false)
    {
    }

    bool AcquisitionRing::pushDropOldest(const AcquisitionSample& sample)
    {
        bool dropped = false;
        if (_count == kAcquisitionQueueDepth)
        {
            _head = (_head + 1) % kAcquisitionQueueDepth;
            --_count;
            if (_producerDropCount != std::numeric_limits<std::uint64_t>::max())
                ++_producerDropCount;
            _producerOverflowPending = true;
            dropped = true;
        }
        const std::size_t writeIndex = (_head + _count) % kAcquisitionQueueDepth;
        _samples[writeIndex] = sample;
        ++_count;
        return dropped;
    }

    bool AcquisitionRing::pop(AcquisitionSample& sample, bool& producerOverflowBeforeSample)
    {
        if (_count == 0)
            return false;
        sample = _samples[_head];
        _head = (_head + 1) % kAcquisitionQueueDepth;
        --_count;
        producerOverflowBeforeSample = _producerOverflowPending;
        _producerOverflowPending = false;
        return true;
    }

    void AcquisitionRing::clear()
    {
        _head = 0;
        _count = 0;
        _producerOverflowPending = false;
    }
} // namespace D2B
