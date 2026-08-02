#pragma once

#include <cstdint>

namespace D2B_HTTPD_LIFECYCLE
{
    enum class Phase : std::uint8_t
    {
        Stopped,
        Running,
        PreStopping,
        StopFailed,
    };

    enum class BeginStopResult : std::uint8_t
    {
        First,
        Retry,
        Repeated,
        WrongHandle,
        Inactive,
    };

    struct Snapshot
    {
        Phase phase;
        std::uintptr_t handleKey;
        std::uint32_t generation;
        bool accepting;
    };

    /*
     * Allocation-free state for one HTTPD server generation.  The device
     * serializes all calls with the D2B send mutex and pipeline critical
     * section; host tests can drive the model directly.
     */
    class State
    {
    public:
        State();

        bool beginRunning(std::uintptr_t handleKey, std::uint32_t generation);
        bool accepting(std::uintptr_t handleKey, std::uint32_t generation) const;
        BeginStopResult beginStop(std::uintptr_t handleKey, std::uint32_t generation);
        bool stopFailure(std::uintptr_t handleKey, std::uint32_t generation);
        bool stopSuccess(std::uintptr_t handleKey, std::uint32_t generation);
        Snapshot snapshot() const;

    private:
        bool matches(std::uintptr_t handleKey, std::uint32_t generation) const;

        Phase _phase;
        std::uintptr_t _handle_key;
        std::uint32_t _generation;
    };
} // namespace D2B_HTTPD_LIFECYCLE
