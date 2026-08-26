/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <string>

namespace WAVEFORM_SCALE
{
    constexpr int kGuideSpacingPixels = 20;
    constexpr int kPlotTopY = 20;
    constexpr int kPlotHeightPixels = 180;
    constexpr int kUpperLabelY = 40;
    constexpr int kMiddleLabelY = 120;
    constexpr int kZeroY = 200;
    constexpr int kScaleReadoutCenterY = 220;
    constexpr int kScaleLabelX = 2;
    constexpr int kMiddleDivisions = 4;
    constexpr int kUpperDivisions = 8;
    constexpr int kPositiveDivisions = 9;

    struct AutoScaleState
    {
        std::size_t scaleIndex = 0;
    };

    struct Range
    {
        float bottom = 0.0f;
        float top = 0.0f;
    };

    std::size_t VoltageScaleCount();
    std::size_t CurrentScaleCount();
    float VoltageValuePerDiv(std::size_t index);
    float CurrentValuePerDiv(std::size_t index);
    std::size_t VisibleSampleSkip(std::size_t sampleCount, std::size_t visibleLimit);
    void UpdateVoltageAutoScale(AutoScaleState& state, float positivePeak);
    void UpdateCurrentAutoScale(AutoScaleState& state, float positivePeak);
    Range PositiveRange(float valuePerDiv);
    Range IncludePositiveFiniteValue(const Range& range, float value);
    float ClampToRange(float value, const Range& range);

    std::string FormatVoltageScaleReadout(std::size_t index);
    std::string FormatVoltageDivisionLabel(std::size_t index, int divisions);
    std::string FormatCurrentScaleReadout(std::size_t index);
    std::string FormatCurrentDivisionLabel(std::size_t index, int divisions);

} // namespace WAVEFORM_SCALE
