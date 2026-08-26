#include "vi_acquisition_timestamp.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace
{
    void ExpectEqual(std::uint64_t actual, std::uint64_t expected, const char* message)
    {
        if (actual != expected)
        {
            std::fprintf(stderr,
                         "FAIL: %s (actual=%llu expected=%llu)\n",
                         message,
                         static_cast<unsigned long long>(actual),
                         static_cast<unsigned long long>(expected));
            std::exit(1);
        }
    }

    void TestMidpoint()
    {
        D2B::ViAcquisitionTimestamp even(1000);
        even.acceptCurrentCandidate(1100);
        ExpectEqual(even.sampleTimestampUs(), 1050, "even interval midpoint");

        D2B::ViAcquisitionTimestamp odd(1000);
        odd.acceptCurrentCandidate(1101);
        ExpectEqual(odd.sampleTimestampUs(), 1050, "odd interval rounds down");
    }

    void TestNearMaximumDoesNotOverflow()
    {
        const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        D2B::ViAcquisitionTimestamp timestamp(maximum - 10);
        timestamp.acceptCurrentCandidate(maximum);
        ExpectEqual(timestamp.sampleTimestampUs(), maximum - 5, "near-maximum midpoint");
    }

    void TestFinalCandidateWins()
    {
        D2B::ViAcquisitionTimestamp lowToHigh(1000);
        lowToHigh.acceptCurrentCandidate(1040); // discarded LC candidate
        lowToHigh.acceptCurrentCandidate(1120); // accepted HC candidate
        ExpectEqual(lowToHigh.sampleTimestampUs(), 1060, "LC to HC uses final candidate");

        D2B::ViAcquisitionTimestamp highToLow(2000);
        highToLow.acceptCurrentCandidate(2060); // discarded HC candidate
        highToLow.acceptCurrentCandidate(2140); // accepted LC candidate
        ExpectEqual(highToLow.sampleTimestampUs(), 2070, "HC to LC uses final candidate");

        D2B::ViAcquisitionTimestamp noRetry(3000);
        noRetry.acceptCurrentCandidate(3080);
        ExpectEqual(noRetry.sampleTimestampUs(), 3040, "no retry uses only candidate");
    }

    void TestRegressionFallsBackToFinalCurrentBoundary()
    {
        D2B::ViAcquisitionTimestamp timestamp(1100);
        timestamp.acceptCurrentCandidate(1000);
        ExpectEqual(timestamp.sampleTimestampUs(), 1000, "regression uses final current acquisition boundary");
    }
} // namespace

int main()
{
    TestMidpoint();
    TestNearMaximumDoesNotOverflow();
    TestFinalCandidateWins();
    TestRegressionFallsBackToFinalCurrentBoundary();
    std::puts("vi_acquisition_timestamp_test: PASS");
    return 0;
}
