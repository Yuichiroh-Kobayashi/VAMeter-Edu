#include "reverse_current_detector.h"

#include <cmath>

namespace REVERSE_CURRENT_DETECTOR
{
    Detector::Detector(const Configuration& configuration)
        : _configuration(configuration),
          _configurationValid(std::isfinite(configuration.negativeThresholdA) && configuration.negativeThresholdA < 0.0F &&
                              configuration.requiredQualifyingCount > 0),
          _state(State::Normal), _qualifyingCount(0)
    {
    }

    bool Detector::isConfigurationValid() const { return _configurationValid; }

    Result Detector::observe(bool measurementValid, float signedCurrentA)
    {
        if (!_configurationValid || _state == State::Latched)
            return result();
        if (!measurementValid || !std::isfinite(signedCurrentA))
            return result();
        if (signedCurrentA <= _configuration.negativeThresholdA)
        {
            ++_qualifyingCount;
            if (_qualifyingCount >= _configuration.requiredQualifyingCount)
            {
                _state = State::Latched;
                return {_state, _qualifyingCount, true};
            }
            _state = State::Candidate;
        }
        else
        {
            _qualifyingCount = 0;
            _state = State::Normal;
        }
        return result();
    }

    Result Detector::result() const { return {_state, _qualifyingCount, false}; }
} // namespace REVERSE_CURRENT_DETECTOR
