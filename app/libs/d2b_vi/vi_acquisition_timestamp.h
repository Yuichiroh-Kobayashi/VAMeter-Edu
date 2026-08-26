#pragma once

#include <cstdint>

namespace D2B
{
    // Selects the logical timestamp for a sequential V/I sample. Current
    // candidates may be replaced when range selection retries; only the last
    // accepted candidate contributes to the result.
    class ViAcquisitionTimestamp
    {
    public:
        explicit ViAcquisitionTimestamp(std::uint64_t voltageCompletionUs)
            : _voltageCompletionUs(voltageCompletionUs), _currentCompletionUs(voltageCompletionUs)
        {
        }

        void acceptCurrentCandidate(std::uint64_t currentCompletionUs) { _currentCompletionUs = currentCompletionUs; }

        std::uint64_t sampleTimestampUs() const
        {
            // esp_timer_get_time() is monotonic within a boot. If that invariant
            // is nevertheless violated, retain the final acquisition boundary;
            // never replace it with a later publication-time clock reading.
            if (_currentCompletionUs < _voltageCompletionUs)
                return _currentCompletionUs;

            // Subtraction first avoids overflowing near UINT64_MAX. Integer
            // division rounds a half-microsecond interval down.
            return _voltageCompletionUs + (_currentCompletionUs - _voltageCompletionUs) / 2;
        }

    private:
        std::uint64_t _voltageCompletionUs;
        std::uint64_t _currentCompletionUs;
    };
} // namespace D2B
