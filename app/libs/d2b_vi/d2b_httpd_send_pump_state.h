#pragma once

#include <cstdint>

namespace D2B_HTTPD_SEND_PUMP
{
    enum class ScheduleResult : std::uint8_t
    {
        Inactive,
        Accepted,
        Coalesced,
    };

    struct ScheduleDecision
    {
        ScheduleResult result;
        std::uintptr_t token;
    };

    enum class BeginResult : std::uint8_t
    {
        Stale,
        Began,
    };

    enum class FinishResult : std::uint8_t
    {
        Stale,
        Idle,
        Rescheduled,
    };

    struct FinishDecision
    {
        FinishResult result;
        std::uintptr_t token;
    };

    struct Snapshot
    {
        bool active;
        std::uint32_t generation;
        bool pending;
        bool executing;
        std::uintptr_t pendingToken;
        std::uintptr_t executingToken;
    };

    /*
     * Allocation-free, non-thread-safe ownership state for one HTTPD work
     * callback.  Device callers serialize every method with the pipeline lock;
     * the host tests drive it directly to make the handoff rules explicit.
     */
    class State
    {
    public:
        State();

        bool activate(std::uint32_t generation);
        bool invalidate(std::uint32_t generation);

        ScheduleDecision request(std::uint32_t generation);

        BeginResult begin(std::uintptr_t token);
        BeginResult begin(std::uint32_t generation, std::uintptr_t token);

        FinishDecision finish(std::uintptr_t token, bool workRemains);
        FinishDecision finish(std::uint32_t generation,
                              std::uintptr_t token,
                              bool workRemains);

        bool reject(std::uintptr_t token);
        bool reject(std::uint32_t generation, std::uintptr_t token);

        Snapshot snapshot() const;

    private:
        std::uintptr_t nextToken();

        bool _active;
        std::uint32_t _generation;
        std::uintptr_t _nextToken;
        std::uintptr_t _pendingToken;
        std::uintptr_t _executingToken;
    };
} // namespace D2B_HTTPD_SEND_PUMP
