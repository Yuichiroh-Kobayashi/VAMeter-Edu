/*
 * SPDX-License-Identifier: MIT
 */
#include "waveform_scale.h"

#include <algorithm>
#include <cmath>

namespace WAVEFORM_SCALE
{
    namespace
    {
        constexpr float kVoltageScales[] = {0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f};
        constexpr float kCurrentScales[] = {
            0.0001f, 0.0002f, 0.0005f, 0.001f, 0.002f, 0.005f, 0.01f,
            0.02f,   0.05f,   0.1f,    0.2f,   0.5f,   1.0f,
        };

        constexpr const char* kVoltageReadouts[] = {
            "0.1V/div", "0.2V/div", "0.5V/div", "1.0V/div", "2V/div", "5V/div",
        };
        constexpr const char* kVoltageMiddleLabels[] = {
            "0.4V", "0.8V", "2.0V", "4.0V", "8V", "20V",
        };
        constexpr const char* kVoltageUpperLabels[] = {
            "0.8V", "1.6V", "4.0V", "8.0V", "16V", "40V",
        };

        constexpr const char* kCurrentReadouts[] = {
            "100uA/div", "200uA/div", "500uA/div", "1.0mA/div", "2mA/div", "5mA/div", "10mA/div",
            "20mA/div",  "50mA/div",  "100mA/div", "0.2A/div",  "0.5A/div", "1.0A/div",
        };
        constexpr const char* kCurrentMiddleLabels[] = {
            "0.4mA", "0.8mA", "2.0mA", "4.0mA", "8mA", "20mA", "40mA",
            "80mA",  "200mA", "400mA", "0.8A",   "2.0A", "4.0A",
        };
        constexpr const char* kCurrentUpperLabels[] = {
            "0.8mA", "1.6mA", "4.0mA", "8.0mA", "16mA", "40mA", "80mA",
            "160mA", "400mA", "800mA", "1.6A",   "4.0A", "8.0A",
        };

        template <std::size_t N>
        std::size_t ClampIndex(std::size_t index, const float (&)[N])
        {
            return std::min(index, N - 1);
        }

        template <std::size_t N>
        void UpdateAutoScale(AutoScaleState& state, float positivePeak, const float (&scales)[N])
        {
            if (!std::isfinite(positivePeak) || positivePeak < 0.0f)
                positivePeak = 0.0f;
            state.scaleIndex = ClampIndex(state.scaleIndex, scales);

            while (state.scaleIndex + 1 < N && positivePeak >= kUpperDivisions * scales[state.scaleIndex])
                ++state.scaleIndex;

            if (state.scaleIndex != 0 && positivePeak < kMiddleDivisions * scales[state.scaleIndex - 1])
                --state.scaleIndex;
        }

        template <std::size_t N>
        std::string LookupLabel(std::size_t index, const char* const (&labels)[N])
        {
            return labels[std::min(index, N - 1)];
        }

    } // namespace

    std::size_t VoltageScaleCount() { return sizeof(kVoltageScales) / sizeof(kVoltageScales[0]); }
    std::size_t CurrentScaleCount() { return sizeof(kCurrentScales) / sizeof(kCurrentScales[0]); }
    float VoltageValuePerDiv(std::size_t index) { return kVoltageScales[ClampIndex(index, kVoltageScales)]; }
    float CurrentValuePerDiv(std::size_t index) { return kCurrentScales[ClampIndex(index, kCurrentScales)]; }
    std::size_t VisibleSampleSkip(std::size_t sampleCount, std::size_t visibleLimit)
    {
        return sampleCount > visibleLimit ? sampleCount - visibleLimit : 0;
    }
    void UpdateVoltageAutoScale(AutoScaleState& state, float positivePeak)
    {
        UpdateAutoScale(state, positivePeak, kVoltageScales);
    }
    void UpdateCurrentAutoScale(AutoScaleState& state, float positivePeak)
    {
        UpdateAutoScale(state, positivePeak, kCurrentScales);
    }

    Range PositiveRange(float valuePerDiv)
    {
        Range range;
        range.top = std::max(0.0f, valuePerDiv) * kPositiveDivisions;
        return range;
    }

    float ClampToRange(float value, const Range& range)
    {
        if (!std::isfinite(value) || !std::isfinite(range.bottom) || !std::isfinite(range.top) || range.top <= range.bottom)
            return range.bottom;
        return std::max(range.bottom, std::min(value, range.top));
    }

    std::string FormatVoltageScaleReadout(std::size_t index) { return LookupLabel(index, kVoltageReadouts); }
    std::string FormatVoltageDivisionLabel(std::size_t index, int divisions)
    {
        return divisions == kMiddleDivisions ? LookupLabel(index, kVoltageMiddleLabels)
                                             : LookupLabel(index, kVoltageUpperLabels);
    }
    std::string FormatCurrentScaleReadout(std::size_t index) { return LookupLabel(index, kCurrentReadouts); }
    std::string FormatCurrentDivisionLabel(std::size_t index, int divisions)
    {
        return divisions == kMiddleDivisions ? LookupLabel(index, kCurrentMiddleLabels)
                                             : LookupLabel(index, kCurrentUpperLabels);
    }

} // namespace WAVEFORM_SCALE
