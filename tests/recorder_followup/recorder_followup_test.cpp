/*
 * SPDX-License-Identifier: MIT
 */
#include "owned_task_resource.h"
#include "record_csv.h"
#include "waveform_scale.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

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

    void TestCsvParsing()
    {
        RECORD_CSV::ParsedLine parsed;
        CHECK(RECORD_CSV::ParseLine("voltage,current,time,capacity,energy\n", parsed) ==
              RECORD_CSV::line_legacy_header);
        CHECK(RECORD_CSV::ParseLine("voltage,current,elapsed_ms,capacity,energy\n", parsed) ==
              RECORD_CSV::line_current_header);
        CHECK(RECORD_CSV::ParseLine("voltage,current,elapsed_ms\n", parsed) == RECORD_CSV::line_current_header);

        CHECK(RECORD_CSV::ParseLine(",,10003,0.0000012,0.0000083\n", parsed) == RECORD_CSV::line_summary);
        CHECK(parsed.recordingDurationMs == 10003);
        CHECK(Near(parsed.capacity, 0.0000012f));
        CHECK(Near(parsed.energy, 0.0000083f));

        CHECK(RECORD_CSV::ParseLine("1.2500,0.0004275\n", parsed) == RECORD_CSV::line_sample);
        CHECK(parsed.hasVoltage);
        CHECK(parsed.hasCurrent);
        CHECK(!parsed.hasElapsedMs);
        CHECK(Near(parsed.voltage, 1.25f));
        CHECK(Near(parsed.current, 0.0004275f));

        CHECK(RECORD_CSV::ParseLine("1.2500,,0\n", parsed) == RECORD_CSV::line_sample);
        CHECK(parsed.hasVoltage);
        CHECK(!parsed.hasCurrent);
        CHECK(parsed.hasElapsedMs);
        CHECK(parsed.elapsedMs == 0);
        CHECK(Near(parsed.current, 0.0f));

        CHECK(RECORD_CSV::ParseLine(",0.0004275,41\n", parsed) == RECORD_CSV::line_sample);
        CHECK(!parsed.hasVoltage);
        CHECK(parsed.hasCurrent);
        CHECK(parsed.elapsedMs == 41);
        CHECK(Near(parsed.voltage, 0.0f));

        const char* rows[] = {
            "0.0000,0.0000000,0\n",
            "0.0125,0.0000025,41,,\n",
            "0.0275,0.0000050,82,,,extra\n",
        };
        std::uint32_t lastElapsed = 0;
        for (std::size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            CHECK(RECORD_CSV::ParseLine(rows[i], parsed) == RECORD_CSV::line_sample);
            CHECK(parsed.hasVoltage);
            CHECK(parsed.hasCurrent);
            CHECK(parsed.hasElapsedMs);
            if (i == 0)
                CHECK(parsed.elapsedMs == 0);
            else
                CHECK(parsed.elapsedMs > lastElapsed);
            lastElapsed = parsed.elapsedMs;
        }

        CHECK(RECORD_CSV::ParseLine("1.0,2.0,not-a-time\n", parsed) == RECORD_CSV::line_invalid);
        CHECK(RECORD_CSV::ParseLine("bad,2.0,3\n", parsed) == RECORD_CSV::line_invalid);
        CHECK(RECORD_CSV::ParseLine("1.0,bad,3\n", parsed) == RECORD_CSV::line_invalid);
        CHECK(RECORD_CSV::ParseLine(",,3\n", parsed) == RECORD_CSV::line_invalid);
        CHECK(RECORD_CSV::ParseLine("1.0,\n", parsed) == RECORD_CSV::line_invalid);
        CHECK(RECORD_CSV::ParseLine("\n", parsed) == RECORD_CSV::line_empty);
    }

    void TestCsvOutput()
    {
        CHECK(std::string(RECORD_CSV::Header()) == "voltage,current,elapsed_ms\n");
        FILE* file = std::tmpfile();
        CHECK(file != nullptr);
        if (file == nullptr)
            return;

        CHECK(RECORD_CSV::WriteHeader(file));
        CHECK(RECORD_CSV::WriteSample(file, RECORD_CSV::output_voltage, 5.125f, -0.25f, 0));
        CHECK(RECORD_CSV::WriteSample(file, RECORD_CSV::output_current, 5.125f, -0.0004275f, 41));
        CHECK(RECORD_CSV::WriteSample(file, RECORD_CSV::output_both, 1.25f, 0.0005f, 82));
        std::rewind(file);

        char line[128] = {0};
        CHECK(std::fgets(line, sizeof(line), file) != nullptr);
        CHECK(std::string(line) == "voltage,current,elapsed_ms\n");
        CHECK(std::fgets(line, sizeof(line), file) != nullptr);
        CHECK(std::string(line) == "5.1250,,0\n");
        CHECK(std::fgets(line, sizeof(line), file) != nullptr);
        CHECK(std::string(line) == ",-0.0004275,41\n");
        CHECK(std::fgets(line, sizeof(line), file) != nullptr);
        CHECK(std::string(line) == "1.2500,0.0005000,82\n");
        CHECK(std::fgets(line, sizeof(line), file) == nullptr); // No summary row.
        std::fclose(file);
    }

    void TestCsvLongLineDrain()
    {
        FILE* file = std::tmpfile();
        CHECK(file != nullptr);
        if (file == nullptr)
            return;

        const std::string longLine(RECORD_CSV::kMaxLineBytes + 20, '7');
        std::fputs((longLine + "\n1.0,2.0,4\n").c_str(), file);
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

    void TestScaleGeometryAndFormatting()
    {
        using namespace WAVEFORM_SCALE;
        CHECK(kGuideSpacingPixels == 20);
        CHECK(kPlotTopY == 20);
        CHECK(kPlotHeightPixels == 180);
        CHECK(kZeroY == 200);
        CHECK(kMiddleLabelY == 120);
        CHECK(kUpperLabelY == 40);
        CHECK(kScaleReadoutCenterY == 220);
        CHECK(kZeroY - kMiddleLabelY == kMiddleDivisions * kGuideSpacingPixels);
        CHECK(kZeroY - kUpperLabelY == kUpperDivisions * kGuideSpacingPixels);
        CHECK(kZeroY - kPlotTopY == kPositiveDivisions * kGuideSpacingPixels);
        CHECK(VisibleSampleSkip(240, 30) == 210);
        CHECK(VisibleSampleSkip(20, 30) == 0);

        const char* voltageReadouts[] = {"0.1V/div", "0.2V/div", "0.5V/div", "1.0V/div", "2V/div", "5V/div"};
        const char* voltageMiddle[] = {"0.4V", "0.8V", "2.0V", "4.0V", "8V", "20V"};
        const char* voltageUpper[] = {"0.8V", "1.6V", "4.0V", "8.0V", "16V", "40V"};
        CHECK(VoltageScaleCount() == sizeof(voltageReadouts) / sizeof(voltageReadouts[0]));
        for (std::size_t i = 0; i < VoltageScaleCount(); ++i)
        {
            CHECK(FormatVoltageScaleReadout(i) == voltageReadouts[i]);
            CHECK(FormatVoltageDivisionLabel(i, kMiddleDivisions) == voltageMiddle[i]);
            CHECK(FormatVoltageDivisionLabel(i, kUpperDivisions) == voltageUpper[i]);
        }

        const char* currentReadouts[] = {
            "100uA/div", "200uA/div", "500uA/div", "1.0mA/div", "2mA/div", "5mA/div", "10mA/div",
            "20mA/div",  "50mA/div",  "100mA/div", "0.2A/div",  "0.5A/div", "1.0A/div",
        };
        const char* currentMiddle[] = {
            "0.4mA", "0.8mA", "2.0mA", "4.0mA", "8mA", "20mA", "40mA",
            "80mA",  "200mA", "400mA", "0.8A",   "2.0A", "4.0A",
        };
        const char* currentUpper[] = {
            "0.8mA", "1.6mA", "4.0mA", "8.0mA", "16mA", "40mA", "80mA",
            "160mA", "400mA", "800mA", "1.6A",   "4.0A", "8.0A",
        };
        CHECK(CurrentScaleCount() == sizeof(currentReadouts) / sizeof(currentReadouts[0]));
        std::string allCurrentLabels;
        for (std::size_t i = 0; i < CurrentScaleCount(); ++i)
        {
            CHECK(FormatCurrentScaleReadout(i) == currentReadouts[i]);
            CHECK(FormatCurrentDivisionLabel(i, kMiddleDivisions) == currentMiddle[i]);
            CHECK(FormatCurrentDivisionLabel(i, kUpperDivisions) == currentUpper[i]);
            allCurrentLabels += FormatCurrentScaleReadout(i);
        }
        CHECK(allCurrentLabels.find("μ") == std::string::npos);
        CHECK(allCurrentLabels.find("µ") == std::string::npos);

        const Range oneVoltRange = PositiveRange(1.0f);
        CHECK(Near(oneVoltRange.bottom, 0.0f));
        CHECK(Near(oneVoltRange.top, 9.0f));
        CHECK(Near(4.0f / 1.0f * kGuideSpacingPixels, static_cast<float>(kZeroY - kMiddleLabelY)));
        CHECK(Near(8.0f / 1.0f * kGuideSpacingPixels, static_cast<float>(kZeroY - kUpperLabelY)));
        CHECK(Near(9.0f / 1.0f * kGuideSpacingPixels, static_cast<float>(kZeroY - kPlotTopY)));
    }

    void TestAutoScale()
    {
        using namespace WAVEFORM_SCALE;
        AutoScaleState voltage;
        AutoScaleState current;
        CHECK(voltage.scaleIndex == 0);
        CHECK(current.scaleIndex == 0);

        const float voltageUpThreshold = kUpperDivisions * VoltageValuePerDiv(0);
        UpdateVoltageAutoScale(voltage, voltageUpThreshold - 0.0001f);
        CHECK(voltage.scaleIndex == 0);
        UpdateVoltageAutoScale(voltage, voltageUpThreshold);
        CHECK(voltage.scaleIndex == 1);

        voltage.scaleIndex = 0;
        UpdateVoltageAutoScale(voltage, 9.0f);
        CHECK(voltage.scaleIndex == 4); // Multi-step: 0.1 -> 0.2 -> 0.5 -> 1.0 -> 2.0.
        UpdateVoltageAutoScale(voltage, 1000.0f);
        CHECK(voltage.scaleIndex == VoltageScaleCount() - 1);

        voltage.scaleIndex = 2;
        const float downThreshold = kMiddleDivisions * VoltageValuePerDiv(1);
        UpdateVoltageAutoScale(voltage, downThreshold);
        CHECK(voltage.scaleIndex == 2);
        UpdateVoltageAutoScale(voltage, downThreshold - 0.0001f);
        CHECK(voltage.scaleIndex == 1);
        voltage.scaleIndex = 2;
        UpdateVoltageAutoScale(voltage, 0.9f); // Visible peak remains above the lower-scale 4-div threshold.
        CHECK(voltage.scaleIndex == 2);

        UpdateCurrentAutoScale(current, kUpperDivisions * CurrentValuePerDiv(0));
        CHECK(current.scaleIndex == 1);
        CHECK(voltage.scaleIndex == 2); // Independent channel state.

        const float negativeMeasurement = -0.25f;
        current.scaleIndex = 0;
        UpdateCurrentAutoScale(current, negativeMeasurement);
        CHECK(current.scaleIndex == 0);
        CHECK(Near(negativeMeasurement, -0.25f));
        const Range currentRange = PositiveRange(CurrentValuePerDiv(current.scaleIndex));
        CHECK(Near(ClampToRange(negativeMeasurement, currentRange), 0.0f));
        CHECK(Near(ClampToRange(1000.0f, currentRange), currentRange.top));
        CHECK(std::isfinite(ClampToRange(std::numeric_limits<float>::infinity(), currentRange)));
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
        CHECK(destructCount == 2);

        CHECK(owner.acquire(std::unique_ptr<TrackedResource>(new TrackedResource(destructCount))));
        owner.taskCreateFailed();
        CHECK(destructCount == 3);

        CHECK(owner.acquire(std::unique_ptr<TrackedResource>(new TrackedResource(destructCount))));
        owner.taskStarted();
        owner.requestStop();
        CHECK(owner.stopRequested());
        CHECK(!owner.releaseFinished());
        CHECK(destructCount == 3);
        CHECK(!owner.acquire(std::unique_ptr<TrackedResource>(new TrackedResource(destructCount))));
        CHECK(destructCount == 4);
        owner.taskFinished();
        CHECK(owner.releaseFinished());
        CHECK(destructCount == 5);
        CHECK(owner.releaseFinished());
        CHECK(destructCount == 5);
    }
} // namespace

int main()
{
    TestCsvParsing();
    TestCsvOutput();
    TestCsvLongLineDrain();
    TestScaleGeometryAndFormatting();
    TestAutoScale();
    TestTriggerOwnership();

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "recorder follow-up tests passed\n";
    return 0;
}
