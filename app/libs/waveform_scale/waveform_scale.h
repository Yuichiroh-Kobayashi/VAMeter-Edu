/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <string>

namespace WAVEFORM_SCALE
{
    constexpr int kChartHeightPixels = 170;
    constexpr int kHorizontalGuideSpacingPixels = 20;

    enum DisplayMode
    {
        mode_both = 0,
        mode_voltage,
        mode_current,
    };

    enum ControlTarget
    {
        target_time = 0,
        target_voltage,
        target_current,
    };

    struct Settings
    {
        ControlTarget target = target_time;
        std::size_t voltageScaleIndex = 0;
        std::size_t currentScaleIndex = 0;
    };

    struct Range
    {
        float bottom = 0.0f;
        float top = 0.0f;
    };

    struct BadgeGeometry
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int textRight = 0;
        int textTop = 0;
    };

    float VisibleDivisions();
    std::size_t VoltageScaleCount();
    std::size_t CurrentScaleCount();
    float VoltageValuePerDiv(std::size_t index);
    float CurrentValuePerDiv(std::size_t index);
    bool IsVoltageAuto(const Settings& settings);
    bool IsCurrentAuto(const Settings& settings);
    bool IsTargetSelected(const Settings& settings, ControlTarget target);
    void CycleTarget(Settings& settings, DisplayMode mode);
    void AdjustSelectedScale(Settings& settings, int direction);
    float FullRangeFromPerDiv(float valuePerDiv);
    float AutoPerDivFromFullRange(float fullRange);
    Range ManualVoltageRange(float valuePerDiv, float observedMinimum);
    Range ManualCurrentRange(float valuePerDiv);
    Range ResolveVoltageRange(const Settings& settings, const Range& autoRange, float observedMinimum);
    Range ResolveCurrentRange(const Settings& settings, const Range& autoRange);
    float ClampToRange(float value, const Range& range);
    std::string FormatVoltagePerDiv(float valuePerDiv);
    std::string FormatCurrentPerDiv(float valuePerDiv);
    std::string FormatVoltageLabel(bool isAuto, float valuePerDiv);
    std::string FormatCurrentLabel(bool isAuto, float valuePerDiv);
    BadgeGeometry MakeRightAlignedBadgeGeometry(int right,
                                                int top,
                                                int textWidth,
                                                int horizontalPadding,
                                                int height,
                                                int textTopPadding);
} // namespace WAVEFORM_SCALE
