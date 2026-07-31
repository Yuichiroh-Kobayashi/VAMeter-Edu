#pragma once

namespace WEB_SERVER_OWNER
{
    enum class Owner
    {
        None,
        System,
        Download,
    };

    class State
    {
    public:
        State();

        bool acquire(Owner owner);
        bool release(Owner owner);
        Owner owner() const;

    private:
        Owner _owner;
    };
} // namespace WEB_SERVER_OWNER
