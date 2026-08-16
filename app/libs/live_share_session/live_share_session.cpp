#include "live_share_session.h"

#include <cctype>

namespace LIVE_SHARE_SESSION
{
    NetworkShareSession::NetworkShareSession()
        : _state(State::Inactive),
          _lastStartOutcome(StartOutcome::Failed),
          _transportStopStatus(TransportStopStatus::Idle),
          _browserConnected(false),
          _measurementTerminated(false),
          _serverStopRequested(false),
          _stopStartedMs(0)
    {
    }

    State NetworkShareSession::state() const { return _state; }

    StartOutcome NetworkShareSession::lastStartOutcome() const { return _lastStartOutcome; }

    TransportStopStatus NetworkShareSession::transportStopStatus() const { return _transportStopStatus; }

    bool NetworkShareSession::browserConnected() const { return _browserConnected; }

    bool NetworkShareSession::measurementTerminated() const { return _measurementTerminated; }

    Action NetworkShareSession::requestStart()
    {
        if (_measurementTerminated || (_state != State::Inactive && _state != State::StartError))
            return Action::None;
        _state = State::Starting;
        _transportStopStatus = TransportStopStatus::Idle;
        _serverStopRequested = false;
        return Action::StartSystemLive;
    }

    void NetworkShareSession::finishStart(StartOutcome outcome)
    {
        if (_state != State::Starting || _measurementTerminated)
            return;
        _lastStartOutcome = outcome;
        _state = outcome == StartOutcome::Started ? State::WifiQr : State::StartError;
    }

    void NetworkShareSession::stationCountObserved(std::uint8_t count)
    {
        if (_state == State::WifiQr && count != 0U)
            _state = State::ViewerQr;
    }

    void NetworkShareSession::manualNext()
    {
        if (_state == State::WifiQr)
            _state = State::ViewerQr;
    }

    void NetworkShareSession::manualPrevious()
    {
        if (_state == State::ViewerQr)
            _state = State::WifiQr;
    }

    void NetworkShareSession::browserConnectionObserved(bool connected)
    {
        _browserConnected = connected;
    }

    Action NetworkShareSession::requestStop(std::uint32_t nowMs)
    {
        if (_measurementTerminated || (_state != State::WifiQr && _state != State::ViewerQr))
            return Action::None;
        _state = State::Stopping;
        _transportStopStatus = TransportStopStatus::Queued;
        _serverStopRequested = false;
        _stopStartedMs = nowMs;
        return Action::BeginNetworkStop;
    }

    Action NetworkShareSession::observeTransportStop(TransportStopStatus status, std::uint32_t nowMs)
    {
        if (_state != State::Stopping || _measurementTerminated)
            return Action::None;
        _transportStopStatus = status;
        if (status == TransportStopStatus::NoOwner || status == TransportStopStatus::Complete ||
            status == TransportStopStatus::Failed || stopDeadlineExpired(nowMs))
        {
            if (_serverStopRequested)
                return Action::None;
            _serverStopRequested = true;
            return Action::StopSystemServer;
        }
        return Action::None;
    }

    Action NetworkShareSession::tick(std::uint32_t nowMs)
    {
        if (_state != State::Stopping || _measurementTerminated || _serverStopRequested ||
            !stopDeadlineExpired(nowMs))
            return Action::None;
        _serverStopRequested = true;
        return Action::StopSystemServer;
    }

    void NetworkShareSession::finishCleanup(CleanupOutcome outcome)
    {
        if ((_state != State::Stopping && _state != State::StopRecovery) || _measurementTerminated)
            return;
        if (outcome == CleanupOutcome::Stopped || outcome == CleanupOutcome::AlreadyStopped)
        {
            _state = State::Inactive;
            _transportStopStatus = TransportStopStatus::Idle;
            _browserConnected = false;
            _serverStopRequested = false;
            return;
        }
        _state = State::StopRecovery;
        _serverStopRequested = false;
    }

    Action NetworkShareSession::retryCleanup()
    {
        if (_state != State::StopRecovery || _measurementTerminated)
            return Action::None;
        return Action::RetryCleanup;
    }

    void NetworkShareSession::measurementTerminationObserved(MeasurementTermination reason)
    {
        (void)reason;
        _measurementTerminated = true;
        _state = State::Inactive;
        _transportStopStatus = TransportStopStatus::Idle;
        _browserConnected = false;
        _serverStopRequested = false;
    }

    bool NetworkShareSession::stopDeadlineExpired(std::uint32_t nowMs) const
    {
        return static_cast<std::uint32_t>(nowMs - _stopStartedMs) >= kOrderlyLocalStopUiDeadlineMs;
    }

    std::string EscapeWifiSsid(const std::string& ssid)
    {
        std::string escaped;
        escaped.reserve(ssid.size());
        for (std::string::const_iterator character = ssid.begin(); character != ssid.end(); ++character)
        {
            if (*character == '\\' || *character == ';' || *character == ',' || *character == ':')
                escaped.push_back('\\');
            escaped.push_back(*character);
        }
        return escaped;
    }

    std::string BuildWifiQrPayload(const std::string& activeApSsid)
    {
        return std::string("WIFI:T:nopass;S:") + EscapeWifiSsid(activeApSsid) + ";;";
    }

    std::string BuildViewerUrl(const std::string& trustedActiveIpv4)
    {
        if (trustedActiveIpv4.empty() || trustedActiveIpv4.size() > 15U)
            return std::string();
        unsigned componentCount = 0;
        unsigned value = 0;
        bool hasDigit = false;
        for (std::size_t index = 0; index <= trustedActiveIpv4.size(); ++index)
        {
            const char character = index == trustedActiveIpv4.size() ? '.' : trustedActiveIpv4[index];
            if (character >= '0' && character <= '9')
            {
                hasDigit = true;
                value = value * 10U + static_cast<unsigned>(character - '0');
                if (value > 255U)
                    return std::string();
            }
            else if (character == '.')
            {
                if (!hasDigit || ++componentCount > 4U)
                    return std::string();
                value = 0;
                hasDigit = false;
            }
            else
            {
                return std::string();
            }
        }
        if (componentCount != 4U)
            return std::string();
        return std::string("http://") + trustedActiveIpv4 + "/viewer/";
    }
} // namespace LIVE_SHARE_SESSION
