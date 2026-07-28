/*
 * SPDX-License-Identifier: MIT
 */
#include "waveform_scale.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace WAVEFORM_SCALE
{
    namespace
    {
        constexpr float kVoltageScales[] = {0.0f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f};
        constexpr float kCurrentScales[] = {0.0f, 0.0001f, 0.0002f, 0.0005f, 0.001f, 0.002f, 0.005f,
                                            0.01f, 0.02f,   0.05f,   0.1f,   0.2f,   0.5f,   1.0f, 2.0f};

        std::string FormatNumber(float value)
        {
            char buffer[24] = {0};
            if (std::fabs(value - std::round(value)) < 0.0001f)
                std::snprintf(buffer, sizeof(buffer), "%.0f", value);
            else if (std::fabs(value * 10.0f - std::round(value * 10.0f)) < 0.0001f)
                std::snprintf(buffer, sizeof(buffer), "%.1f", value);
            else
                std::snprintf(buffer, sizeof(buffer), "%.2f", value);
            return buffer;
        }

        void AdjustIndex(std::size_t& index, std::size_t count, int direction)
        {
            if (direction == 0 || count < 2)
                return;
            if (index == 0)
            {
                if (direction > 0)
                    index = 1;
                return;
            }
            if (direction > 0 && index + 1 < count)
                ++index;
            else if (direction < 0)
                --index;
        }
    } // namespace

    float VisibleDivisions()
    {
        return static_cast<float>(kChartHeightPixels) / static_cast<float>(kHorizontalGuideSpacingPixels);
    }

    std::size_t VoltageScaleCount() { return sizeof(kVoltageScales) / sizeof(kVoltageScales[0]); }
    std::size_t CurrentScaleCount() { return sizeof(kCurrentScales) / sizeof(kCurrentScales[0]); }
    float VoltageValuePerDiv(std::size_t index) { return kVoltageScales[std::min(index, VoltageScaleCount() - 1)]; }
    float CurrentValuePerDiv(std::size_t index) { return kCurrentScales[std::min(index, CurrentScaleCount() - 1)]; }
    bool IsVoltageAuto(const Settings& settings) { return settings.voltageScaleIndex == 0; }
    bool IsCurrentAuto(const Settings& settings) { return settings.currentScaleIndex == 0; }

    void CycleTarget(Settings& settings, DisplayMode mode)
    {
        if (mode == mode_voltage)
            settings.target = settings.target == target_time ? target_voltage : target_time;
        else if (mode == mode_current)
            settings.target = settings.target == target_time ? target_current : target_time;
        else if (settings.target == target_time)
            settings.target = target_voltage;
        else if (settings.target == target_voltage)
            settings.target = target_current;
        else
            settings.target = target_time;
    }

    void AdjustSelectedScale(Settings& settings, int direction)
    {
        if (settings.target == target_voltage)
            AdjustIndex(settings.voltageScaleIndex, VoltageScaleCount(), direction);
        else if (settings.target == target_current)
            AdjustIndex(settings.currentScaleIndex, CurrentScaleCount(), direction);
    }

    float FullRangeFromPerDiv(float valuePerDiv) { return std::max(0.0f, valuePerDiv) * VisibleDivisions(); }
    float AutoPerDivFromFullRange(float fullRange) { return std::max(0.0f, fullRange) / VisibleDivisions(); }

    Range ManualVoltageRange(float valuePerDiv, float observedMinimum)
    {
        Range range;
        const float fullRange = FullRangeFromPerDiv(valuePerDiv);
        if (observedMinimum < 0.0f)
        {
            range.bottom = -fullRange / 2.0f;
            range.top = fullRange / 2.0f;
        }
        else
            range.top = fullRange;
        return range;
    }

    Range ManualCurrentRange(float valuePerDiv)
    {
        Range range;
        const float halfRange = FullRangeFromPerDiv(valuePerDiv) / 2.0f;
        range.bottom = -halfRange;
        range.top = halfRange;
        return range;
    }

    Range ResolveVoltageRange(const Settings& settings, const Range& autoRange, float observedMinimum)
    {
        if (IsVoltageAuto(settings))
            return autoRange;
        return ManualVoltageRange(VoltageValuePerDiv(settings.voltageScaleIndex), observedMinimum);
    }

    Range ResolveCurrentRange(const Settings& settings, const Range& autoRange)
    {
        if (IsCurrentAuto(settings))
            return autoRange;
        return ManualCurrentRange(CurrentValuePerDiv(settings.currentScaleIndex));
    }

    float ClampToRange(float value, const Range& range)
    {
        if (!std::isfinite(value) || !std::isfinite(range.bottom) || !std::isfinite(range.top) || range.top <= range.bottom)
            return 0.0f;
        return std::max(range.bottom, std::min(value, range.top));
    }

    std::string FormatVoltagePerDiv(float valuePerDiv) { return FormatNumber(valuePerDiv) + "V/div"; }

    std::string FormatCurrentPerDiv(float valuePerDiv)
    {
        const float magnitude = std::fabs(valuePerDiv);
        if (magnitude < 0.001f)
            return FormatNumber(valuePerDiv * 1000000.0f) + "uA/div";
        if (magnitude < 1.0f)
            return FormatNumber(valuePerDiv * 1000.0f) + "mA/div";
        return FormatNumber(valuePerDiv) + "A/div";
    }

    std::string FormatVoltageLabel(bool isAuto, float valuePerDiv)
    {
        return isAuto ? "V AUTO " + FormatVoltagePerDiv(valuePerDiv) : "V " + FormatVoltagePerDiv(valuePerDiv);
    }

    std::string FormatCurrentLabel(bool isAuto, float valuePerDiv)
    {
        return isAuto ? "I AUTO " + FormatCurrentPerDiv(valuePerDiv) : "I " + FormatCurrentPerDiv(valuePerDiv);
    }
} // namespace WAVEFORM_SCALE
