#pragma once

#include <cstdint>

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
        explicit State(std::uint32_t generationSeed = 0);

        bool acquire(Owner owner);
        bool release(Owner owner);
        Owner owner() const;
        std::uint32_t generation() const;

    private:
        Owner _owner;
        std::uint32_t _generation;
    };
} // namespace WEB_SERVER_OWNER
