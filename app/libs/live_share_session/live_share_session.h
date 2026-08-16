#pragma once

#include <cstdint>
#include <string>

namespace LIVE_SHARE_SESSION
{
    static const std::uint32_t kOrderlyLocalStopUiDeadlineMs = 1000U;

    enum class State : std::uint8_t
    {
        Inactive,
        Starting,
        WifiQr,
        ViewerQr,
        Stopping,
        StopRecovery,
        StartError,
    };

    enum class Action : std::uint8_t
    {
        None,
        StartSystemLive,
        BeginNetworkStop,
        StopSystemServer,
        RetryCleanup,
    };

    enum class StartOutcome : std::uint8_t
    {
        Started,
        Busy,
        Failed,
    };

    enum class TransportStopStatus : std::uint8_t
    {
        Idle,
        Queued,
        NoOwner,
        OwnerClosing,
        ActiveStreamStopping,
        Complete,
        Failed,
    };

    enum class CleanupOutcome : std::uint8_t
    {
        Stopped,
        AlreadyStopped,
        RetryRequired,
        RejectedWrongOwner,
        ApStopFailed,
    };

    enum class MeasurementTermination : std::uint8_t
    {
        Exit,
        Fault,
    };

    class NetworkShareSession
    {
    public:
        NetworkShareSession();

        State state() const;
        StartOutcome lastStartOutcome() const;
        TransportStopStatus transportStopStatus() const;
        bool browserConnected() const;
        bool measurementTerminated() const;

        Action requestStart();
        void finishStart(StartOutcome outcome);
        void stationCountObserved(std::uint8_t count);
        void manualNext();
        void manualPrevious();
        void browserConnectionObserved(bool connected);

        Action requestStop(std::uint32_t nowMs);
        Action observeTransportStop(TransportStopStatus status, std::uint32_t nowMs);
        Action tick(std::uint32_t nowMs);
        void finishCleanup(CleanupOutcome outcome);
        Action retryCleanup();
        void measurementTerminationObserved(MeasurementTermination reason);

    private:
        bool stopDeadlineExpired(std::uint32_t nowMs) const;

        State _state;
        StartOutcome _lastStartOutcome;
        TransportStopStatus _transportStopStatus;
        bool _browserConnected;
        bool _measurementTerminated;
        bool _serverStopRequested;
        std::uint32_t _stopStartedMs;
    };

    std::string EscapeWifiSsid(const std::string& ssid);
    std::string BuildWifiQrPayload(const std::string& activeApSsid);
    std::string BuildViewerUrl(const std::string& trustedActiveIpv4);
} // namespace LIVE_SHARE_SESSION
