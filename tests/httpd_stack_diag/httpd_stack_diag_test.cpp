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

    void ObserveRequiredStages(HTTPD_STACK_DIAG::State& state,
                               std::uint32_t actualStackBytes,
                               HTTPD_STACK_DIAG::Stage skipped = HTTPD_STACK_DIAG::Stage::NONE)
    {
        using namespace HTTPD_STACK_DIAG;
        SetConfiguredStackBytes(state, actualStackBytes);
        for (std::uint32_t value = 1U; value < static_cast<std::uint32_t>(Stage::COUNT); ++value)
        {
            const Stage stage = static_cast<Stage>(value);
            if (stage == skipped)
                continue;
            Update(state, stage, 2048U - value, 1U, actualStackBytes, 0x1234U, 1U, value);
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
    Check(StageBit(Stage::NONE) == 0U, "NONE stage bit invalid");
    Check(StageBit(Stage::HTTP_REQUEST_ENTER) == 1U, "stage 1 bit");
    Check(StageBit(Stage::WS_CLOSE_COMPLETE) == (1U << 18U), "stage 19 bit");
    Check(StageBit(Stage::COUNT) == 0U, "COUNT stage bit invalid");
    Check(RequiredStageMask() == ((1U << 19U) - 1U), "required mask all 19 stages");

    SetConfiguredStackBytes(state, 4096U);
    Update(state, Stage::HTTP_REQUEST_ENTER, 1800U, 1U, 4096U, 0x1234U, 1U, 0U);
    Update(state, Stage::CONTROL_PARSE_COMPLETE, 1400U, 1U, 4096U, 0x1234U, 2U, 7U);
    Update(state, Stage::HTTP_REQUEST_ENTER, 1600U, 1U, 4096U, 0x1234U, 3U, 8U);
    Check(state.stages[StageIndex(Stage::HTTP_REQUEST_ENTER)].minimum_observed_bytes == 1600U, "stage minimum selection");
    Check(state.global_minimum_bytes == 1400U, "global minimum selection");
    Check(state.minimum_first_observed_stage == static_cast<std::uint32_t>(Stage::CONTROL_PARSE_COMPLETE),
          "minimum first observed stage");
    Check(MeasurementConsistent(state), "partial measurement internally consistent");
    Check(!RequiredStagesComplete(state), "partial required stages incomplete");
    Check(!UsableForAcceptance(state), "partial stages rejected for acceptance");
    Check(NormalizeHighWater(123U, 4U) == 492U, "unit normalization");
    Check(NormalizeHighWater(std::numeric_limits<std::uint32_t>::max(), 4U) == std::numeric_limits<std::uint32_t>::max(),
          "normalization saturation");

    State zero = {};
    Reset(zero);
    SetConfiguredStackBytes(zero, 4096U);
    Update(zero, Stage::WELCOME_SEND_RETURN, 0U, 1U, 4096U, 0x1234U, 1U, 0U);
    Check(!UsableForAcceptance(zero), "zero rejected");

    State mismatch = {};
    Reset(mismatch);
    SetConfiguredStackBytes(mismatch, 4096U);
    Update(mismatch, Stage::HTTP_REQUEST_ENTER, 100U, 1U, 4096U, 1U, 0U, 0U);
    Update(mismatch, Stage::WS_FRAME_RECEIVE_ENTER, 90U, 1U, 4096U, 2U, 0U, 0U);
    Check(!UsableForAcceptance(mismatch), "task mismatch rejected");

    State complete = {};
    Reset(complete);
    ObserveRequiredStages(complete, 4096U);
    Check(complete.configured_stack_mismatch == 0U, "actual 4096 matches expected");
    Check(MeasurementConsistent(complete), "complete measurement consistent");
    Check(RequiredStagesComplete(complete), "all required stages complete");
    Check(UsableForAcceptance(complete), "complete measurement accepted");

    State missingOne = {};
    Reset(missingOne);
    ObserveRequiredStages(missingOne, 4096U, Stage::WELCOME_SEND_ENTER);
    Check(!RequiredStagesComplete(missingOne), "one required stage missing");
    Check(!UsableForAcceptance(missingOne), "missing one stage rejected");

    State wrongConfig = {};
    Reset(wrongConfig);
    ObserveRequiredStages(wrongConfig, 8192U);
    Check(wrongConfig.configured_stack_mismatch != 0U, "actual 8192 mismatch");
    Check(!MeasurementConsistent(wrongConfig), "actual 8192 inconsistent");
    Check(!UsableForAcceptance(wrongConfig), "actual 8192 rejected");

    State unsetConfig = {};
    Reset(unsetConfig);
    Update(unsetConfig, Stage::HTTP_REQUEST_ENTER, 100U, 1U, 0U, 1U, 0U, 0U);
    Check(!MeasurementConsistent(unsetConfig), "unset actual config inconsistent");
    Check(!UsableForAcceptance(unsetConfig), "unset actual config rejected");

    State zeroConfig = {};
    Reset(zeroConfig);
    SetConfiguredStackBytes(zeroConfig, 0U);
    Update(zeroConfig, Stage::HTTP_REQUEST_ENTER, 100U, 1U, 0U, 1U, 0U, 0U);
    Check(zeroConfig.configured_stack_mismatch != 0U, "zero actual config mismatch");
    Check(!UsableForAcceptance(zeroConfig), "zero actual config rejected");

    State repeatedLifecycle = {};
    Reset(repeatedLifecycle);
    ObserveRequiredStages(repeatedLifecycle, 4096U);
    SetConfiguredStackBytes(repeatedLifecycle, 4096U);
    Check(repeatedLifecycle.lifecycle_mismatch != 0U, "second lifecycle detected");
    Check(!UsableForAcceptance(repeatedLifecycle), "second lifecycle rejected");

    Breadcrumb first = MakeBreadcrumb(state, Stage::CONTROL_PARSE_COMPLETE, 1U, 3U, 12U);
    Breadcrumb committed = {};
    Commit(committed, first);
    Check(IsValid(committed), "breadcrumb valid");
    Check(committed.magic == kBreadcrumbMagic && committed.version == kBreadcrumbVersion, "breadcrumb magic version");
    Check(committed.actual_configured_stack_bytes == 4096U, "breadcrumb actual config");
    Breadcrumb oldVersion = committed;
    oldVersion.version = 1U;
    oldVersion.checksum = BreadcrumbChecksum(oldVersion);
    Check(!IsValid(oldVersion), "breadcrumb old version rejected");
    Breadcrumb corrupted = committed;
    ++corrupted.actual_configured_stack_bytes;
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
