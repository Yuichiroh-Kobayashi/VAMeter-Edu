#include "httpd_stack_diag.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace
{
    void Check(bool condition, const char* name)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << name << '\n';
            std::exit(1);
        }
    }
} // namespace

int main()
{
    using namespace HTTPD_STACK_DIAG;
    State state = {};
    Reset(state);
    Check(!UsableForAcceptance(state), "uninitialized rejected");
    Check(state.global_minimum_bytes == kUnmeasured, "reset minimum");

    Update(state, Stage::HTTP_REQUEST_ENTER, 1800U, 1U, 4096U, 0x1234U, 1U, 0U);
    Update(state, Stage::CONTROL_PARSE_COMPLETE, 1400U, 1U, 4096U, 0x1234U, 2U, 7U);
    Update(state, Stage::HTTP_REQUEST_ENTER, 1600U, 1U, 4096U, 0x1234U, 3U, 8U);
    Check(state.stages[StageIndex(Stage::HTTP_REQUEST_ENTER)].minimum_observed_bytes == 1600U, "stage minimum selection");
    Check(state.global_minimum_bytes == 1400U, "global minimum selection");
    Check(state.global_minimum_stage == static_cast<std::uint32_t>(Stage::CONTROL_PARSE_COMPLETE), "global minimum stage");
    Check(UsableForAcceptance(state), "initialized accepted");
    Check(NormalizeHighWater(123U, 4U) == 492U, "unit normalization");
    Check(NormalizeHighWater(std::numeric_limits<std::uint32_t>::max(), 4U) == std::numeric_limits<std::uint32_t>::max(),
          "normalization saturation");

    State zero = {};
    Reset(zero);
    Update(zero, Stage::WELCOME_SEND_RETURN, 0U, 1U, 4096U, 0x1234U, 1U, 0U);
    Check(!UsableForAcceptance(zero), "zero rejected");

    State mismatch = {};
    Reset(mismatch);
    Update(mismatch, Stage::HTTP_REQUEST_ENTER, 100U, 1U, 4096U, 1U, 0U, 0U);
    Update(mismatch, Stage::WS_FRAME_RECEIVE_ENTER, 90U, 1U, 4096U, 2U, 0U, 0U);
    Check(!UsableForAcceptance(mismatch), "task mismatch rejected");

    Breadcrumb first = MakeBreadcrumb(state, Stage::CONTROL_PARSE_COMPLETE, 1U, 3U, 12U);
    Breadcrumb committed = {};
    Commit(committed, first);
    Check(IsValid(committed), "breadcrumb valid");
    Check(committed.magic == kBreadcrumbMagic && committed.version == kBreadcrumbVersion, "breadcrumb magic version");
    Breadcrumb corrupted = committed;
    ++corrupted.minimum_observed_bytes;
    Check(!IsValid(corrupted), "breadcrumb checksum");
    Clear(corrupted);
    Check(!IsValid(corrupted), "breadcrumb clear");

    Breadcrumb secondSource = MakeBreadcrumb(state, Stage::WELCOME_SEND_RETURN, 2U, 3U, 12U);
    Breadcrumb second = {};
    Commit(second, secondSource);
    bool haveValid = false;
    Check(SelectNewest(committed, second, haveValid) == 1U && haveValid, "newest slot");
    Check(NextSequence(std::numeric_limits<std::uint32_t>::max(), true) == 1U, "sequence wrap");
    Check(IsSequenceNewer(1U, std::numeric_limits<std::uint32_t>::max()), "wrapped sequence newer");
    Check(std::string(StageName(Stage::VIOLATION_RESPONSE_3)) == "VIOLATION_RESPONSE_3", "stage token");

    std::cout << "httpd_stack_diag_test: PASS\n";
    return 0;
}
