#include "d2b_httpd_lifecycle_state.h"

namespace D2B_HTTPD_LIFECYCLE
{
    State::State() : _phase(Phase::Stopped), _handle_key(0), _generation(0) {}

    bool State::matches(std::uintptr_t handleKey, std::uint32_t generation) const
    {
        return handleKey != 0 && generation != 0 && handleKey == _handle_key && generation == _generation;
    }

    bool State::beginRunning(std::uintptr_t handleKey, std::uint32_t generation)
    {
        if (handleKey == 0 || generation == 0 || _phase != Phase::Stopped)
            return false;

        _handle_key = handleKey;
        _generation = generation;
        _phase = Phase::Running;
        return true;
    }

    bool State::accepting(std::uintptr_t handleKey, std::uint32_t generation) const
    {
        return _phase == Phase::Running && matches(handleKey, generation);
    }

    BeginStopResult State::beginStop(std::uintptr_t handleKey, std::uint32_t generation)
    {
        if (handleKey == 0 || generation == 0)
            return BeginStopResult::Inactive;
        if (_phase == Phase::Stopped)
            return BeginStopResult::Inactive;
        if (!matches(handleKey, generation))
            return BeginStopResult::WrongHandle;

        if (_phase == Phase::Running)
        {
            _phase = Phase::PreStopping;
            return BeginStopResult::First;
        }
        if (_phase == Phase::StopFailed)
        {
            _phase = Phase::PreStopping;
            return BeginStopResult::Retry;
        }
        return BeginStopResult::Repeated;
    }

    bool State::stopFailure(std::uintptr_t handleKey, std::uint32_t generation)
    {
        if (_phase != Phase::PreStopping || !matches(handleKey, generation))
            return false;
        _phase = Phase::StopFailed;
        return true;
    }

    bool State::stopSuccess(std::uintptr_t handleKey, std::uint32_t generation)
    {
        if ((_phase != Phase::PreStopping && _phase != Phase::StopFailed) ||
            !matches(handleKey, generation))
            return false;
        _phase = Phase::Stopped;
        _handle_key = 0;
        _generation = 0;
        return true;
    }

    Snapshot State::snapshot() const
    {
        return {_phase, _handle_key, _generation, _phase == Phase::Running};
    }
} // namespace D2B_HTTPD_LIFECYCLE
