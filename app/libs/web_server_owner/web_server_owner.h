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
        void markRetained();
        bool retained() const;
        Owner owner() const;
        std::uint32_t generation() const;

    private:
        Owner _owner;
        std::uint32_t _generation;
        bool _retained;
    };
} // namespace WEB_SERVER_OWNER
