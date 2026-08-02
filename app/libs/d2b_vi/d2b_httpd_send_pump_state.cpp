#include "d2b_httpd_send_pump_state.h"

namespace D2B_HTTPD_SEND_PUMP
{
    State::State() : _active(false), _generation(0), _nextToken(0), _pendingToken(0), _executingToken(0) {}

    bool State::activate(std::uint32_t generation)
    {
        if (generation == 0)
            return false;

        _active = true;
        _generation = generation;
        _pendingToken = 0;
        _executingToken = 0;
        return true;
    }

    bool State::invalidate(std::uint32_t generation)
    {
        if (!_active || generation == 0 || generation != _generation)
            return false;

        _active = false;
        _generation = 0;
        _pendingToken = 0;
        _executingToken = 0;
        return true;
    }

    ScheduleDecision State::request(std::uint32_t generation)
    {
        if (!_active || generation == 0 || generation != _generation)
            return {ScheduleResult::Inactive, 0};

        if (_pendingToken != 0 || _executingToken != 0)
            return {ScheduleResult::Coalesced, 0};

        const std::uintptr_t token = nextToken();
        _pendingToken = token;
        return {ScheduleResult::Accepted, token};
    }

    BeginResult State::begin(std::uintptr_t token)
    {
        if (!_active || token == 0 || _pendingToken != token)
            return BeginResult::Stale;

        _pendingToken = 0;
        _executingToken = token;
        return BeginResult::Began;
    }

    BeginResult State::begin(std::uint32_t generation, std::uintptr_t token)
    {
        if (!_active || generation == 0 || generation != _generation)
            return BeginResult::Stale;
        return begin(token);
    }

    FinishDecision State::finish(std::uintptr_t token, bool workRemains)
    {
        if (!_active || token == 0 || _executingToken != token)
            return {FinishResult::Stale, 0};

        _executingToken = 0;
        if (!workRemains)
            return {FinishResult::Idle, 0};

        const std::uintptr_t next = nextToken();
        _pendingToken = next;
        return {FinishResult::Rescheduled, next};
    }

    FinishDecision State::finish(std::uint32_t generation,
                                 std::uintptr_t token,
                                 bool workRemains)
    {
        if (!_active || generation == 0 || generation != _generation)
            return {FinishResult::Stale, 0};
        return finish(token, workRemains);
    }

    bool State::reject(std::uintptr_t token)
    {
        if (!_active || token == 0 || _pendingToken != token)
            return false;

        _pendingToken = 0;
        return true;
    }

    bool State::reject(std::uint32_t generation, std::uintptr_t token)
    {
        if (!_active || generation == 0 || generation != _generation)
            return false;
        return reject(token);
    }

    Snapshot State::snapshot() const
    {
        return {_active,
                _generation,
                _pendingToken != 0,
                _executingToken != 0,
                _pendingToken,
                _executingToken};
    }

    std::uintptr_t State::nextToken()
    {
        // Zero is reserved.  A current token is never returned again while
        // it can still be observed by a callback.  On a 32-bit uintptr_t a
        // mathematically perfect lifetime guarantee is not possible after a
        // full wrap; the device additionally compares the active generation
        // while holding the pipeline lock, and only one current token can be
        // pending/executing at a time.
        do
        {
            ++_nextToken;
            if (_nextToken == 0)
                ++_nextToken;
        } while (_nextToken == _pendingToken || _nextToken == _executingToken);
        return _nextToken;
    }
} // namespace D2B_HTTPD_SEND_PUMP
