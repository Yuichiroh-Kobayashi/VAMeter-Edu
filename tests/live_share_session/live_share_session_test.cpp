#include "live_share_session.h"

#include <cstdlib>
#include <iostream>

namespace LSS = LIVE_SHARE_SESSION;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    void StartToWifi(LSS::NetworkShareSession& session)
    {
        Require(session.requestStart() == LSS::Action::StartSystemLive, "explicit start action");
        session.finishStart(LSS::StartOutcome::Started);
        Require(session.state() == LSS::State::WifiQr, "start success enters Wi-Fi QR");
    }

    void TestHappyPathAndQrPolicy()
    {
        LSS::NetworkShareSession session;
        Require(session.state() == LSS::State::Inactive, "initial inactive");
        StartToWifi(session);
        session.manualNext();
        Require(session.state() == LSS::State::ViewerQr, "manual next enters viewer QR");
        session.manualPrevious();
        Require(session.state() == LSS::State::WifiQr, "viewer encoder returns to Wi-Fi QR");
        session.stationCountObserved(1);
        Require(session.state() == LSS::State::ViewerQr, "station join enters viewer QR");
        Require(session.requestStop(100) == LSS::Action::BeginNetworkStop, "local stop begins network stop");
        Require(session.observeTransportStop(LSS::TransportStopStatus::Complete, 200) == LSS::Action::StopSystemServer,
                "orderly completion stops server");
        session.finishCleanup(LSS::CleanupOutcome::Stopped);
        Require(session.state() == LSS::State::Inactive, "cleanup returns to measurement");
    }

    void TestStartFailuresAndRepeatedStart()
    {
        LSS::NetworkShareSession failed;
        Require(failed.requestStart() == LSS::Action::StartSystemLive, "failure start request");
        Require(failed.requestStart() == LSS::Action::None, "repeated start while starting ignored");
        failed.finishStart(LSS::StartOutcome::AllocationOrListenFailure);
        Require(failed.state() == LSS::State::StartError, "start failure enters error");
        Require(failed.requestStart() == LSS::Action::None, "error cannot directly restart");
        Require(failed.dismissStartError(), "side dismiss is an explicit transition");
        Require(failed.state() == LSS::State::Inactive, "dismiss returns inactive");
        Require(failed.requestStart() == LSS::Action::StartSystemLive, "a later separate start is accepted");
        failed.finishStart(LSS::StartOutcome::Started);
        Require(failed.state() == LSS::State::WifiQr, "later start can succeed");
        Require(failed.requestStart() == LSS::Action::None, "start while active ignored");

        LSS::NetworkShareSession busy;
        busy.requestStart();
        busy.finishStart(LSS::StartOutcome::BusyOtherOwner);
        Require(busy.state() == LSS::State::StartError && busy.lastStartOutcome() == LSS::StartOutcome::BusyOtherOwner,
                "busy remains a typed visible error");
        Require(busy.requestStart() == LSS::Action::None, "busy presentation blocks restart");
        Require(busy.dismissStartError(), "busy can be dismissed");
        Require(busy.state() == LSS::State::Inactive, "busy dismiss unlocks later use");
        Require(!busy.dismissStartError(), "inactive cannot be dismissed again");
    }

    void TestTypedStartDebt()
    {
        const LSS::StartOutcome retained[] = {
            LSS::StartOutcome::RetainedApNeedsStopRetry,
            LSS::StartOutcome::RetainedServerNeedsStopRetry,
        };
        for (std::size_t index = 0; index < sizeof(retained) / sizeof(retained[0]); ++index)
        {
            LSS::NetworkShareSession session;
            Require(session.requestStart() == LSS::Action::StartSystemLive, "retained failure start request");
            session.finishStart(retained[index]);
            Require(session.state() == LSS::State::StopRecovery, "retained start result enters cleanup recovery");
            Require(session.requestStart() == LSS::Action::None, "retained start debt blocks restart");
            Require(!session.dismissStartError(), "retained debt cannot dismiss past cleanup");
            Require(session.retryCleanup() == LSS::Action::RetryCleanup, "retained debt exposes only cleanup retry");
            session.finishCleanup(LSS::CleanupOutcome::RetryRequired);
            Require(session.state() == LSS::State::StopRecovery, "retained cleanup failure remains locked");
            session.finishCleanup(LSS::CleanupOutcome::AlreadyStopped);
            Require(session.state() == LSS::State::Inactive, "typed cleanup success unlocks retained debt");
        }
    }

    void TestBrowserIsObservational()
    {
        LSS::NetworkShareSession session;
        StartToWifi(session);
        session.browserConnectionObserved(true);
        Require(session.browserConnected() && session.state() == LSS::State::WifiQr, "browser connect is observational");
        session.browserConnectionObserved(false);
        Require(!session.browserConnected() && session.state() == LSS::State::WifiQr,
                "browser disconnect does not stop local session");
    }

    void TestOwnerCasesAndTimeout()
    {
        LSS::NetworkShareSession noOwner;
        StartToWifi(noOwner);
        noOwner.requestStop(10);
        Require(noOwner.observeTransportStop(LSS::TransportStopStatus::NoOwner, 11) == LSS::Action::StopSystemServer,
                "no owner proceeds without fake exchange");

        LSS::NetworkShareSession noStream;
        StartToWifi(noStream);
        noStream.requestStop(20);
        Require(noStream.observeTransportStop(LSS::TransportStopStatus::OwnerClosing, 21) == LSS::Action::None,
                "owner without stream closes before server");
        Require(noStream.observeTransportStop(LSS::TransportStopStatus::Complete, 22) == LSS::Action::StopSystemServer,
                "owner close completion proceeds");

        LSS::NetworkShareSession active;
        StartToWifi(active);
        active.requestStop(1000);
        Require(active.observeTransportStop(LSS::TransportStopStatus::ActiveStreamStopping, 1500) == LSS::Action::None,
                "active stream awaits orderly end");
        Require(active.observeTransportStop(LSS::TransportStopStatus::Complete, 1900) == LSS::Action::StopSystemServer,
                "active stream orderly completion proceeds");

        LSS::NetworkShareSession timeout;
        StartToWifi(timeout);
        timeout.requestStop(0xffffff00U);
        Require(timeout.tick(0x000002e7U) == LSS::Action::None, "deadline wrap-safe before 1000 ms");
        Require(timeout.tick(0x000002e8U) == LSS::Action::StopSystemServer, "1000 ms deadline triggers fallback");
        Require(timeout.tick(0x000002e9U) == LSS::Action::None, "fallback requested once");
    }

    void TestCleanupRecovery()
    {
        const LSS::CleanupOutcome failures[] = {
            LSS::CleanupOutcome::RetryRequired,
            LSS::CleanupOutcome::RejectedWrongOwner,
            LSS::CleanupOutcome::ApStopFailed,
        };
        for (std::size_t index = 0; index < sizeof(failures) / sizeof(failures[0]); ++index)
        {
            LSS::NetworkShareSession session;
            StartToWifi(session);
            session.requestStop(0);
            Require(session.observeTransportStop(LSS::TransportStopStatus::NoOwner, 1) == LSS::Action::StopSystemServer,
                    "cleanup failure setup");
            session.finishCleanup(failures[index]);
            Require(session.state() == LSS::State::StopRecovery, "typed cleanup failure enters recovery");
            Require(session.retryCleanup() == LSS::Action::RetryCleanup, "recovery retry only cleans up");
            session.finishCleanup(failures[index]);
            Require(session.state() == LSS::State::StopRecovery, "retry failure remains recovery");
            Require(session.requestStart() == LSS::Action::None, "recovery never restarts sharing");
            Require(session.retryCleanup() == LSS::Action::RetryCleanup, "second retry allowed");
            session.finishCleanup(LSS::CleanupOutcome::AlreadyStopped);
            Require(session.state() == LSS::State::Inactive, "retry success returns to measurement");
        }
    }

    void TestC2aDominance()
    {
        const LSS::State entryStates[] = {
            LSS::State::WifiQr,
            LSS::State::ViewerQr,
            LSS::State::Stopping,
        };
        for (std::size_t index = 0; index < sizeof(entryStates) / sizeof(entryStates[0]); ++index)
        {
            for (unsigned reason = 0; reason < 2; ++reason)
            {
                LSS::NetworkShareSession session;
                StartToWifi(session);
                if (entryStates[index] == LSS::State::ViewerQr)
                    session.manualNext();
                if (entryStates[index] == LSS::State::Stopping)
                    session.requestStop(10);
                session.measurementTerminationObserved(reason == 0 ? LSS::MeasurementTermination::Exit
                                                                   : LSS::MeasurementTermination::Fault);
                Require(session.state() == LSS::State::Inactive && session.measurementTerminated(),
                        "C2A termination dominates share state");
                Require(session.requestStart() == LSS::Action::None, "no resurrection after C2A termination");
                Require(session.tick(5000) == LSS::Action::None, "no ordinary cleanup after C2A termination");
            }
        }
    }

    void TestRelayAndModeInvariants()
    {
        for (unsigned initialRelay = 0; initialRelay < 2; ++initialRelay)
        {
            bool relay = initialRelay != 0;
            int mode = 2;
            int range = 7;
            LSS::NetworkShareSession session;
            StartToWifi(session);
            session.requestStop(1);
            session.observeTransportStop(LSS::TransportStopStatus::Complete, 2);
            session.finishCleanup(LSS::CleanupOutcome::Stopped);
            Require(relay == (initialRelay != 0), "ordinary stop leaves relay unchanged");
            Require(mode == 2 && range == 7, "ordinary stop leaves mode and range unchanged");
        }
    }

    void TestQrPayloads()
    {
        Require(LSS::BuildWifiQrPayload("Class\\A;B,C:D") == "WIFI:T:nopass;S:Class\\\\A\\;B\\,C\\:D;;",
                "Wi-Fi QR exact escaping");
        Require(LSS::BuildWifiQrPayload("M5-VAMeter-07") == "WIFI:T:nopass;S:M5-VAMeter-07;;", "Wi-Fi QR exact payload");
        Require(LSS::BuildViewerUrl("192.168.4.1") == "http://192.168.4.1/viewer/", "trusted viewer URL exact payload");
        Require(LSS::BuildViewerUrl("256.1.1.1").empty(), "reject invalid IPv4 authority");
        Require(LSS::BuildViewerUrl("host.example").empty(), "reject hostname authority");
    }
} // namespace

int main()
{
    TestHappyPathAndQrPolicy();
    TestStartFailuresAndRepeatedStart();
    TestTypedStartDebt();
    TestBrowserIsObservational();
    TestOwnerCasesAndTimeout();
    TestCleanupRecovery();
    TestC2aDominance();
    TestRelayAndModeInvariants();
    TestQrPayloads();
    std::cout << "live_share_session_test PASS\n";
}
