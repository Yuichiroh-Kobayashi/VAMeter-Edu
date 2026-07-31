#include "web_server_owner.h"

namespace WEB_SERVER_OWNER
{
    State::State() : _owner(Owner::None) {}

    bool State::acquire(Owner owner)
    {
        if (owner == Owner::None || _owner != Owner::None)
            return false;

        _owner = owner;
        return true;
    }

    bool State::release(Owner owner)
    {
        if (owner == Owner::None || _owner != owner)
            return false;

        _owner = Owner::None;
        return true;
    }

    Owner State::owner() const { return _owner; }
} // namespace WEB_SERVER_OWNER
