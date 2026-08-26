#include "reverse_current_detector.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace
{
    using namespace REVERSE_CURRENT_DETECTOR;
    const Configuration kConfig = {-0.25F, 3};
    void Expect(bool value, const char* message)
    {
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", message);
            std::exit(1);
        }
    }
    void Check(const Result& result, State state, std::uint32_t count, bool entered, const char* message)
    {
        Expect(result.state == state && result.qualifyingCount == count && result.enteredLatched == entered, message);
    }
    void TestValuesAndLatch()
    {
        Detector detector(kConfig);
        Expect(detector.isConfigurationValid(), "configuration valid");
        Check(detector.observe(true, 2.0F), State::Normal, 0, false, "positive");
        Check(detector.observe(true, 0.0F), State::Normal, 0, false, "zero");
        Check(detector.observe(true, -0.249F), State::Normal, 0, false, "shallow negative");
        Check(detector.observe(true, -0.25F), State::Candidate, 1, false, "exact threshold");
        Check(detector.observe(true, -0.5F), State::Candidate, 2, false, "second qualifier");
        Check(detector.observe(true, -std::numeric_limits<float>::max()), State::Latched, 3, true, "third latches");
        Check(detector.observe(true, 1.0F), State::Latched, 3, false, "latched stays");
        Check(detector.observe(false, -1.0F), State::Latched, 3, false, "invalid cannot clear");
        Check(detector.observe(true, -1.0F), State::Latched, 3, false, "event exactly once");
    }
    void TestResetAndInvalidHold()
    {
        Detector reset(kConfig);
        Check(reset.observe(true, -0.5F), State::Candidate, 1, false, "candidate");
        Check(reset.observe(true, 0.0F), State::Normal, 0, false, "reset");
        Check(reset.observe(true, -0.5F), State::Candidate, 1, false, "restart");
        Detector hold(kConfig);
        Check(hold.observe(true, -0.5F), State::Candidate, 1, false, "candidate hold");
        Check(hold.observe(false, 0.0F), State::Candidate, 1, false, "no invalid-to-zero");
        Check(hold.observe(true, -0.5F), State::Candidate, 2, false, "continue after invalid");
        Check(hold.observe(false, -100.0F), State::Candidate, 2, false, "invalid does not increment");
        Check(hold.observe(true, -0.5F), State::Latched, 3, true, "third valid latches");
    }
    void TestConfiguration()
    {
        Detector one({-1.0F, 1});
        Check(one.observe(true, -1.0F), State::Latched, 1, true, "explicit N");
        const Configuration invalid[] = {{-0.25F, 0},
                                         {0.0F, 3},
                                         {0.25F, 3},
                                         {std::numeric_limits<float>::infinity(), 3},
                                         {-std::numeric_limits<float>::infinity(), 3},
                                         {std::numeric_limits<float>::quiet_NaN(), 3}};
        for (const Configuration& configuration : invalid)
        {
            Detector detector(configuration);
            Expect(!detector.isConfigurationValid(), "invalid configuration");
            Check(detector.observe(true, -std::numeric_limits<float>::max()), State::Normal, 0, false, "fails closed");
        }
    }
} // namespace

int main()
{
    TestValuesAndLatch();
    TestResetAndInvalidHold();
    TestConfiguration();
    std::puts("reverse_current_detector_test: PASS");
}
