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

    enum class ApModeState : std::uint8_t
    {
        Disabled,
        Enabled,
        Unknown,
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
    typedef ApModeState (*ApModeQueryCallback)(void* context);

    struct ApOperationCallbacks
    {
        void* context;
        ApOperationCallback isStaConnected;
        ApOperationCallback disconnectSta;
        ApModeQueryCallback queryApMode;
        ApOperationCallback startAp;
        ApOperationCallback stopAp;
    };

    class ApOperation
    {
    public:
        ApOperation();

        bool reconcileStartPreflight(const ApOperationCallbacks& callbacks);
        ApStartResult start(const ApOperationCallbacks& callbacks);
        ApStopResult stop(const ApOperationCallbacks& callbacks);
        ApState state() const;
        bool stopRetryRequired() const;

    private:
        ApState _state;
    };
} // namespace WEB_SERVER_OWNER
