#pragma once

#include <cmath>

namespace CURRENT_WAVEFORM_CLIP
{
    struct Sample
    {
        float x = 0.0f;
        float current = 0.0f;
        bool valid = false;
    };

    struct Segment
    {
        Sample start;
        Sample end;
    };

    inline bool ClipToPositiveDomain(const Sample& first, const Sample& second, Segment& output)
    {
        if (!first.valid || !second.valid || !std::isfinite(first.x) || !std::isfinite(first.current) ||
            !std::isfinite(second.x) || !std::isfinite(second.current))
            return false;
        if (first.current < 0.0f && second.current < 0.0f)
            return false;
        output.start = first;
        output.end = second;
        if (first.current < 0.0f)
        {
            const float fraction = -first.current / (second.current - first.current);
            output.start.x = first.x + (second.x - first.x) * fraction;
            output.start.current = 0.0f;
        }
        else if (second.current < 0.0f)
        {
            const float fraction = -first.current / (second.current - first.current);
            output.end.x = first.x + (second.x - first.x) * fraction;
            output.end.current = 0.0f;
        }
        return true;
    }
} // namespace CURRENT_WAVEFORM_CLIP
