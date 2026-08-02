#pragma once

#include <cstdint>

namespace WEB_SERVER_OWNER
{
    enum class ApState : std::uint8_t
    {
        Inactive,
        Active,
        StopRetryRequired,
    };

    enum class ApStartResult : std::uint8_t
    {
        Started,
        StaDisconnectFailed,
        StartFailed,
        StopRetryRequired,
    };

    enum class ApStopResult : std::uint8_t
    {
        Stopped,
        AlreadyStopped,
        StopFailed,
    };

    typedef bool (*ApOperationCallback)(void* context);

    struct ApOperationCallbacks
    {
        void* context;
        ApOperationCallback isStaConnected;
        ApOperationCallback disconnectSta;
        ApOperationCallback isApStarted;
        ApOperationCallback startAp;
        ApOperationCallback stopAp;
    };

    class ApOperation
    {
    public:
        ApOperation();

        ApStartResult start(const ApOperationCallbacks& callbacks);
        ApStopResult stop(const ApOperationCallbacks& callbacks);
        ApState state() const;
        bool stopRetryRequired() const;

    private:
        ApState _state;
    };
} // namespace WEB_SERVER_OWNER
