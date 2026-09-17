/*
 * SPDX-License-Identifier: MIT
 */
#include "recorder_sample_buffer.h"

#include <limits>

namespace RECORDER_SAMPLE_BUFFER
{
    namespace
    {
        bool CheckedMultiply(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t& product)
        {
            if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs)
                return false;
            product = lhs * rhs;
            return true;
        }

        bool CeilDivide(std::uint64_t numerator, std::uint64_t denominator, std::uint64_t& result)
        {
            if (denominator == 0)
                return false;
            result = numerator / denominator;
            if (numerator % denominator != 0)
            {
                if (result == std::numeric_limits<std::uint64_t>::max())
                    return false;
                ++result;
            }
            return true;
        }
    } // namespace

    SampleBuffer::SampleBuffer(RecordedSample* storage, std::size_t capacity)
        : storage_(storage), size_(0), capacity_(capacity), frozen_(false)
    {
    }

    AppendResult SampleBuffer::append(const RecordedSample& sample)
    {
        if (frozen_)
            return append_frozen;
        if (storage_ == nullptr || size_ == capacity_)
            return append_full;
        storage_[size_] = sample;
        ++size_;
        return append_accepted;
    }

    void SampleBuffer::freeze() { frozen_ = true; }
    bool SampleBuffer::frozen() const { return frozen_; }
    std::size_t SampleBuffer::size() const { return size_; }
    std::size_t SampleBuffer::capacity() const { return capacity_; }
    const RecordedSample* SampleBuffer::data() const { return size_ == 0 ? nullptr : storage_; }

    CapacityResult CalculateCurrentProductCapacity(std::uint64_t recordMs, std::uint64_t sampleIntervalMs, std::uint64_t tickHz)
    {
        CapacityResult result = {capacity_arithmetic_overflow, 0, 0};
        if (tickHz == 0)
        {
            result.status = capacity_invalid_tick_rate;
            return result;
        }

        std::uint64_t delayNumerator = 0;
        std::uint64_t recordNumerator = 0;
        if (!CheckedMultiply(sampleIntervalMs, tickHz, delayNumerator) || !CheckedMultiply(recordMs, tickHz, recordNumerator))
            return result;

        const std::uint64_t delayTicks = delayNumerator / 1000U;
        if (delayTicks < 2)
        {
            result.status = capacity_delay_too_short;
            return result;
        }

        std::uint64_t recordTicks = 0;
        std::uint64_t required = 0;
        if (!CeilDivide(recordNumerator, 1000U, recordTicks) || !CeilDivide(recordTicks, delayTicks - 1U, required) ||
            required == std::numeric_limits<std::uint64_t>::max())
            return result;
        ++required;
        if (required > kReviewedCapacity)
        {
            result.status = capacity_exceeds_reviewed_cap;
            return result;
        }
        result.status = capacity_ok;
        result.requiredMinimum = static_cast<std::size_t>(required);
        result.selectedCapacity = kReviewedCapacity;
        return result;
    }
} // namespace RECORDER_SAMPLE_BUFFER
