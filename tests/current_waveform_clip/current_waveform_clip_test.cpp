#include "current_waveform_clip.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace
{
    using CURRENT_WAVEFORM_CLIP::Sample;
    using CURRENT_WAVEFORM_CLIP::Segment;
    void Expect(bool c, const char* m)
    {
        if (!c)
        {
            std::fprintf(stderr, "FAIL: %s\n", m);
            std::exit(1);
        }
    }
    bool Near(float a, float e) { return std::fabs(a - e) < 0.00001f; }
    Sample Valid(float x, float c)
    {
        Sample s;
        s.x = x;
        s.current = c;
        s.valid = true;
        return s;
    }
    bool Clip(const Sample& a, const Sample& b, Segment& s) { return CURRENT_WAVEFORM_CLIP::ClipToPositiveDomain(a, b, s); }
} // namespace
int main()
{
    Segment s;
    Expect(Clip(Valid(0, 1), Valid(1, 2), s) && Near(s.start.current, 1) && Near(s.end.current, 2), "positive to positive");
    Expect(Clip(Valid(0, 1), Valid(1, 0), s) && Near(s.end.current, 0), "positive to zero");
    Expect(Clip(Valid(0, 0), Valid(1, 1), s) && Near(s.start.current, 0), "zero to positive");
    Expect(Clip(Valid(2, 2), Valid(6, -2), s) && Near(s.end.x, 4) && Near(s.end.current, 0), "positive to negative");
    Expect(!Clip(Valid(0, -1), Valid(1, -2), s), "negative to negative");
    Expect(Clip(Valid(2, -3), Valid(10, 1), s) && Near(s.start.x, 8) && Near(s.start.current, 0), "negative to positive");
    Expect(Clip(Valid(0, 0), Valid(1, -1), s) && Near(s.start.x, 0) && Near(s.end.x, 0), "zero to negative");
    Expect(Clip(Valid(0, -1), Valid(1, 0), s) && Near(s.start.x, 1) && Near(s.end.x, 1), "negative to zero");
    Sample invalid;
    Expect(!Clip(Valid(0, 1), invalid, s) && !Clip(invalid, Valid(2, 1), s), "invalid breaks segment");
    Expect(!Clip(Valid(0, 1), Valid(1, std::numeric_limits<float>::infinity()), s) &&
               !Clip(Valid(0, std::numeric_limits<float>::quiet_NaN()), Valid(1, 1), s),
           "non-finite");
    Expect(!Clip(Valid(0, -1), Valid(1, -1), s) && Clip(Valid(1, -1), Valid(2, 1), s) && Near(s.start.x, 1.5f),
           "no zero bridge");
    std::puts("current_waveform_clip_test: PASS");
}
