#include "relay_interlock.h"

namespace RELAY_INTERLOCK
{
    RelayInterlock::RelayInterlock() : _initialized(false), _faultLatched(false) {}

    void RelayInterlock::initialize() { _initialized = true; }

    Command RelayInterlock::requestNormal(bool state) const
    {
        if (!state)
            return {true, false};
        return {_initialized && !_faultLatched, true};
    }

    Command RelayInterlock::commitFault()
    {
        if (!_initialized || _faultLatched)
            return {false, false};

        // The sole re-enable latch commits before the caller receives OFF.
        _faultLatched = true;
        return {true, false};
    }

    bool RelayInterlock::isInitialized() const { return _initialized; }

    bool RelayInterlock::isFaultLatched() const { return _faultLatched; }
} // namespace RELAY_INTERLOCK
