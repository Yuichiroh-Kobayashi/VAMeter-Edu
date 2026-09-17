/*
 * SPDX-License-Identifier: MIT
 */
#include "recorder_sample_buffer.h"

#include <cstdint>
#include <iostream>
#include <limits>

namespace
{
    int failures = 0;
    void Check(bool condition, const char* expression, int line)
    {
        if (!condition)
        {
            std::cerr << "line " << line << ": CHECK failed: " << expression << '\n';
            ++failures;
        }
    }
#define CHECK(expression) Check((expression), #expression, __LINE__)

    void TestLayoutAndBoundedStorage()
    {
        using namespace RECORDER_SAMPLE_BUFFER;
        CHECK(sizeof(RecordedSample) == 12);
        CHECK(alignof(RecordedSample) == 4);
        CHECK(kReviewedBufferBytes == kReviewedCapacity * sizeof(RecordedSample));

        SampleBuffer buffer(2);
        const RecordedSample first = {-1.25f, -0.0004275f, 73};
        const RecordedSample second = {9.5f, 7.25f, 12};
        const RecordedSample overflow = {99.0f, 99.0f, 99};
        CHECK(buffer.append(first) == append_accepted);
        CHECK(buffer.append(second) == append_accepted);
        CHECK(buffer.append(overflow) == append_full);
        CHECK(buffer.size() == 2);
        CHECK(buffer.data()[0].voltage == first.voltage);
        CHECK(buffer.data()[0].current == first.current);
        CHECK(buffer.data()[0].elapsedMs == first.elapsedMs);
        CHECK(buffer.data()[1].elapsedMs == 12); // No monotonicity rewrite.
        buffer.freeze();
        CHECK(buffer.frozen());
        CHECK(buffer.append(overflow) == append_frozen);
        CHECK(buffer.size() == 2);
    }

    void TestCurrentCapacityProof()
    {
        using namespace RECORDER_SAMPLE_BUFFER;
        const CapacityResult current = CalculateCurrentProductCapacity(5000, 40, 1000);
        CHECK(current.status == capacity_ok);
        CHECK(current.requiredMinimum == 130);
        CHECK(current.selectedCapacity == 256);

        CHECK(CalculateCurrentProductCapacity(5000, 40, 0).status == capacity_invalid_tick_rate);
        CHECK(CalculateCurrentProductCapacity(5000, 1, 1000).status == capacity_delay_too_short);
        CHECK(CalculateCurrentProductCapacity(5000, 40, 25).status == capacity_delay_too_short);
        CHECK(CalculateCurrentProductCapacity(std::numeric_limits<std::uint64_t>::max(), 40, 1000).status ==
              capacity_arithmetic_overflow);
        CHECK(CalculateCurrentProductCapacity(5000, std::numeric_limits<std::uint64_t>::max(), 1000).status ==
              capacity_arithmetic_overflow);
        CHECK(CalculateCurrentProductCapacity(20000, 40, 1000).status == capacity_exceeds_reviewed_cap);
    }
} // namespace

int main()
{
    TestLayoutAndBoundedStorage();
    TestCurrentCapacityProof();
    if (failures != 0)
        return 1;
    std::cout << "recorder sample buffer tests passed\n";
    return 0;
}
