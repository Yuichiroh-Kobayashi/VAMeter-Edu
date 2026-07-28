/*
 * SPDX-License-Identifier: MIT
 */
#include "owned_task_resource.h"
#include "record_csv.h"
#include "waveform_scale.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void Check(bool condition, const char* expression, int line)
    {
        if (condition)
            return;
        std::cerr << "line " << line << ": CHECK failed: " << expression << '\n';
        ++failures;
    }

#define CHECK(expression) Check((expression), #expression, __LINE__)

    bool Near(float lhs, float rhs, float tolerance = 0.000001f)
    {
        return std::fabs(lhs - rhs) <= tolerance;
    }

    void TestCsvRows()
    {
        RECORD_CSV::ParsedLine parsed;
        CHECK(RECORD_CSV::ParseLine("voltage,current,time,capacity,energy\n", parsed) ==
              RECORD_CSV::line_legacy_header);
        CHECK(RECORD_CSV::ParseLine("voltage,current,elapsed_ms,capacity,energy\n", parsed) ==
              RECORD_CSV::line_current_header);
        CHECK(RECORD_CSV::ParseLine(",,10003,0.0000012,0.0000083\n", parsed) == RECORD_CSV::line_summary);
        CHECK(parsed.recordingDurationMs == 10003);
        CHECK(Near(parsed.capacity, 0.0000012f));
        CHECK(Near(parsed.energy, 0.0000083f));

        CHECK(RECORD_CSV::ParseLine("1.2500,0.0004275\n", parsed) == RECORD_CSV::line_sample);
        CHECK(!parsed.hasElapsedMs);
        CHECK(Near(parsed.voltage, 1.25f));
        CHECK(Near(parsed.current, 0.0004275f));

        const char* rows[] = {
            "0.0000,0.0000000,0,,\n",
            "0.0125,0.0000025,41,,\n",
            "0.0275,0.0000050,82,,\n",
        };
        std::uint32_t lastElapsed = 0;
        for (std::size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            CHECK(RECORD_CSV::ParseLine(rows[i], parsed) == RECORD_CSV::line_sample);
            CHECK(parsed.hasElapsedMs);
            if (i == 0)
                CHECK(parsed.elapsedMs == 0);
            else
                CHECK(parsed.elapsedMs > lastElapsed);
            lastElapsed = parsed.elapsedMs;
        }

        CHECK(RECORD_CSV::ParseLine("1.0,2.0,not-a-time,,\n", parsed) == RECORD_CSV::line_invalid);
        CHECK(RECORD_CSV::ParseLine("1.0,,2,,\n", parsed) == RECORD_CSV::line_invalid);
        CHECK(RECORD_CSV::ParseLine("1.0,2.0,3,,,extra,column\n", parsed) == RECORD_CSV::line_sample);
        CHECK(parsed.elapsedMs == 3);
        CHECK(RECORD_CSV::ParseLine("\n", parsed) == RECORD_CSV::line_empty);
    }

    void TestCsvLongLineDrain()
    {
        FILE* file = std::tmpfile();
        CHECK(file != nullptr);
        if (file == nullptr)
            return;

        const std::string longLine(RECORD_CSV::kMaxLineBytes + 20, '7');
        std::fputs((longLine + "\n1.0,2.0,4,,\n").c_str(), file);
        std::rewind(file);

        char buffer[RECORD_CSV::kMaxLineBytes] = {0};
        bool tooLong = false;
        CHECK(RECORD_CSV::ReadLine(file, buffer, sizeof(buffer), tooLong));
        CHECK(tooLong);
        CHECK(RECORD_CSV::ReadLine(file, buffer, sizeof(buffer), tooLong));
        CHECK(!tooLong);
        RECORD_CSV::ParsedLine parsed;
        CHECK(RECORD_CSV::ParseLine(buffer, parsed) == RECORD_CSV::line_sample);
        CHECK(parsed.elapsedMs == 4);
        std::fclose(file);
    }

    void TestScaleCalculations()
    {
        using namespace WAVEFORM_SCALE;
        Settings settings;
        CHECK(IsVoltageAuto(settings));
        CHECK(IsCurrentAuto(settings));

        CycleTarget(settings, mode_voltage);
        CHECK(settings.target == target_voltage);
        AdjustSelectedScale(settings, 1);
        CHECK(settings.voltageScaleIndex == 1);
        CHECK(!IsVoltageAuto(settings));
        AdjustSelectedScale(settings, -1);
        CHECK(IsVoltageAuto(settings));

        for (std::size_t i = 0; i < VoltageScaleCount() + 4; ++i)
            AdjustSelectedScale(settings, 1);
        CHECK(settings.voltageScaleIndex == VoltageScaleCount() - 1);
        for (std::size_t i = 0; i < VoltageScaleCount() + 4; ++i)
            AdjustSelectedScale(settings, -1);
        CHECK(settings.voltageScaleIndex == 0);

        settings.target = target_current;
        for (std::size_t i = 0; i < CurrentScaleCount() + 4; ++i)
            AdjustSelectedScale(settings, 1);
        CHECK(settings.currentScaleIndex == CurrentScaleCount() - 1);
        for (std::size_t i = 0; i < CurrentScaleCount() + 4; ++i)
            AdjustSelectedScale(settings, -1);
        CHECK(settings.currentScaleIndex == 0);

        CHECK(Near(VisibleDivisions(), 8.5f));
        CHECK(Near(FullRangeFromPerDiv(1.0f), 8.5f));
        CHECK(Near(AutoPerDivFromFullRange(8.5f), 1.0f));
        CHECK(FormatVoltagePerDiv(0.1f) == "0.1V/div");
        CHECK(FormatVoltagePerDiv(1.0f) == "1V/div");
        CHECK(FormatCurrentPerDiv(0.0001f) == "100uA/div");
        CHECK(FormatCurrentPerDiv(0.001f) == "1mA/div");
        CHECK(FormatCurrentPerDiv(0.1f) == "100mA/div");
        CHECK(FormatCurrentPerDiv(1.0f) == "1A/div");

        settings = Settings();
        CycleTarget(settings, mode_both);
        CHECK(settings.target == target_voltage);
        CycleTarget(settings, mode_both);
        CHECK(settings.target == target_current);
        CycleTarget(settings, mode_both);
        CHECK(settings.target == target_time);
        CycleTarget(settings, mode_current);
        CHECK(settings.target == target_current);
        CycleTarget(settings, mode_current);
        CHECK(settings.target == target_time);

        settings.voltageScaleIndex = 4; // 1 V/div
        Range autoVoltage;
        autoVoltage.bottom = 4.0f;
        autoVoltage.top = 6.0f;
        const Range manualVoltage = ResolveVoltageRange(settings, autoVoltage, 0.0f);
        CHECK(Near(manualVoltage.bottom, 0.0f));
        CHECK(Near(manualVoltage.top, 8.5f));
        autoVoltage.bottom = -100.0f;
        autoVoltage.top = 100.0f;
        const Range stillManualVoltage = ResolveVoltageRange(settings, autoVoltage, 0.0f);
        CHECK(Near(stillManualVoltage.top, 8.5f));
        CHECK(manualVoltage.top > 5.13f); // REC-005 acceptance value.

        settings.currentScaleIndex = 2; // 0.2 mA/div
        Range autoCurrent;
        autoCurrent.bottom = -10.0f;
        autoCurrent.top = 10.0f;
        const Range manualCurrent = ResolveCurrentRange(settings, autoCurrent);
        CHECK(Near(manualCurrent.bottom, -0.00085f));
        CHECK(Near(manualCurrent.top, 0.00085f));
        CHECK(manualCurrent.top > 0.0005700f); // REC-003 acceptance value.

        CHECK(Near(ClampToRange(50.0f, manualVoltage), manualVoltage.top));
        CHECK(Near(ClampToRange(-50.0f, manualVoltage), manualVoltage.bottom));
        CHECK(std::isfinite(ClampToRange(1.0e20f, manualVoltage)));
    }

    struct TrackedResource
    {
        explicit TrackedResource(int& destructCount) : destruct_count(destructCount) {}
        ~TrackedResource() { ++destruct_count; }
        int& destruct_count;
    };

    void TestTriggerOwnership()
    {
        using RECORDER_LIFECYCLE::OwnedTaskResource;
        int destructCount = 0;
        OwnedTaskResource<TrackedResource> owner;

        CHECK(owner.acquire(std::unique_ptr<TrackedResource>(new TrackedResource(destructCount))));
        owner.taskStarted();
        owner.taskFinished();
        CHECK(owner.releaseFinished());
        CHECK(destructCount == 1);
        CHECK(owner.releaseFinished());
        CHECK(destructCount == 1);

        {
            std::unique_ptr<TrackedResource> rejected(new TrackedResource(destructCount));
        }
        CHECK(destructCount == 2); // Failure before ownership transfer.

        CHECK(owner.acquire(std::unique_ptr<TrackedResource>(new TrackedResource(destructCount))));
        owner.taskCreateFailed();
        CHECK(destructCount == 3);

        CHECK(owner.acquire(std::unique_ptr<TrackedResource>(new TrackedResource(destructCount))));
        owner.taskStarted();
        owner.requestStop();
        CHECK(owner.stopRequested());
        CHECK(!owner.releaseFinished()); // Destroy timeout keeps trigger alive.
        CHECK(destructCount == 3);
        CHECK(!owner.acquire(std::unique_ptr<TrackedResource>(new TrackedResource(destructCount))));
        CHECK(destructCount == 4); // Rejected new recorder owns and frees only its candidate.
        owner.taskFinished();
        CHECK(owner.releaseFinished());
        CHECK(destructCount == 5);
        CHECK(owner.releaseFinished());
        CHECK(destructCount == 5);
    }
} // namespace

int main()
{
    TestCsvRows();
    TestCsvLongLineDrain();
    TestScaleCalculations();
    TestTriggerOwnership();

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "recorder follow-up tests passed\n";
    return 0;
}
