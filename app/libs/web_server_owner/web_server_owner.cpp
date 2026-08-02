#include "web_server_owner.h"

#include <limits>

namespace WEB_SERVER_OWNER
{
    State::State(std::uint32_t generationSeed) : _owner(Owner::None), _generation(generationSeed), _retained(false) {}

    bool State::acquire(Owner owner)
    {
        if (owner == Owner::None || _owner != Owner::None)
            return false;

        if (_generation == std::numeric_limits<std::uint32_t>::max())
            _generation = 0;
        ++_generation;
        if (_generation == 0)
            ++_generation;
        _owner = owner;
        _retained = false;
        return true;
    }

    bool State::release(Owner owner)
    {
        if (owner == Owner::None || _owner != owner)
            return false;

        _owner = Owner::None;
        _retained = false;
        return true;
    }

    void State::markRetained() { _retained = true; }

    bool State::retained() const { return _retained; }

    Owner State::owner() const { return _owner; }

    std::uint32_t State::generation() const { return _generation; }
} // namespace WEB_SERVER_OWNER
