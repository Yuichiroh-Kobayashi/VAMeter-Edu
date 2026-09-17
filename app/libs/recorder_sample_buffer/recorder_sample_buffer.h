/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace RECORDER_SAMPLE_BUFFER
{
    struct RecordedSample
    {
        float voltage;
        float current;
        std::uint32_t elapsedMs;
    };

    static_assert(sizeof(RecordedSample) == 12, "RecordedSample memory budget changed");
    static_assert(alignof(RecordedSample) == 4, "RecordedSample alignment changed");

    constexpr std::size_t kReviewedCapacity = 256;
    constexpr std::size_t kReviewedBufferBytes = kReviewedCapacity * sizeof(RecordedSample);

    enum AppendResult
    {
        append_accepted = 0,
        append_full,
        append_frozen,
    };

    class SampleBuffer
    {
    public:
        SampleBuffer(RecordedSample* storage, std::size_t capacity);

        AppendResult append(const RecordedSample& sample);
        void freeze();
        bool frozen() const;
        std::size_t size() const;
        std::size_t capacity() const;
        const RecordedSample* data() const;

    private:
        RecordedSample* storage_;
        std::size_t size_;
        std::size_t capacity_;
        bool frozen_;
    };

    enum CapacityStatus
    {
        capacity_ok = 0,
        capacity_invalid_tick_rate,
        capacity_delay_too_short,
        capacity_arithmetic_overflow,
        capacity_exceeds_reviewed_cap,
    };

    struct CapacityResult
    {
        CapacityStatus status;
        std::size_t requiredMinimum;
        std::size_t selectedCapacity;
    };

    // This proof is deliberately specific to the current periodic-delay design.
    // One tick is reserved for tick-boundary conversion uncertainty, and the
    // extra sample includes the initial sample at the start of the interval.
    CapacityResult
    CalculateCurrentProductCapacity(std::uint64_t recordMs, std::uint64_t sampleIntervalMs, std::uint64_t tickHz);
} // namespace RECORDER_SAMPLE_BUFFER
