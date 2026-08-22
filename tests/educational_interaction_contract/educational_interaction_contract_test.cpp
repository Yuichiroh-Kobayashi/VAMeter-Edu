#include "libs/live_share_controller/live_share_controller.h"
#include "libs/meter_help_qr/meter_help_qr_urls.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace LSC = LIVE_SHARE_CONTROLLER;
namespace LSS = LIVE_SHARE_SESSION;
namespace MHQ = METER_HELP_QR;
namespace WSO = WEB_SERVER_OWNER;

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

    struct FakeTransport
    {
        WSO::StartResult startResult = WSO::StartResult::Started;
        WSO::StopResult stopResult = WSO::StopResult::Stopped;
        LSS::TransportStopStatus beginResult = LSS::TransportStopStatus::Complete;
        LSS::TransportStopStatus pollResult = LSS::TransportStopStatus::Complete;
        std::uint32_t now = 1U;
        std::uint8_t stations = 0U;
        int startCalls = 0;
        int beginCalls = 0;
        int pollCalls = 0;
        int finishCalls = 0;

        static WSO::StartResult Start(void* context)
        {
            FakeTransport& fake = *static_cast<FakeTransport*>(context);
            ++fake.startCalls;
            return fake.startResult;
        }

        static LSS::TransportStopStatus Begin(void* context)
        {
            FakeTransport& fake = *static_cast<FakeTransport*>(context);
            ++fake.beginCalls;
            return fake.beginResult;
        }

        static LSS::TransportStopStatus Poll(void* context)
        {
            FakeTransport& fake = *static_cast<FakeTransport*>(context);
            ++fake.pollCalls;
            return fake.pollResult;
        }

        static WSO::StopResult Finish(void* context)
        {
            FakeTransport& fake = *static_cast<FakeTransport*>(context);
            ++fake.finishCalls;
            return fake.stopResult;
        }

        static std::uint32_t Millis(void* context) { return static_cast<FakeTransport*>(context)->now; }

        static std::uint8_t StationCount(void* context) { return static_cast<FakeTransport*>(context)->stations; }

        LSC::TransportCallbacks callbacks()
        {
            LSC::TransportCallbacks value;
            value.context = this;
            value.startSystemLive = Start;
            value.beginSystemLiveStop = Begin;
            value.pollSystemLiveStop = Poll;
            value.finishSystemLiveStop = Finish;
            value.millis = Millis;
            value.stationCount = StationCount;
            return value;
        }
    };

    LSC::InputSnapshot SideClick()
    {
        LSC::InputSnapshot input;
        input.sideClicked = true;
        return input;
    }

    LSC::InputSnapshot EncoderClick()
    {
        LSC::InputSnapshot input;
        input.encoderClicked = true;
        return input;
    }

    void TestMeterHelpUrlsAndGeometry()
    {
        Require(std::strcmp(MHQ::SelectHelpUrl(MHQ::MeterHelpKind::Voltage), MHQ::VOLTAGE_METER_HELP_URL) == 0,
                "U01 voltage selection uses voltage constant");
        Require(std::strcmp(MHQ::VOLTAGE_METER_HELP_URL,
                            "https://digi-keirin.com/js25/jrika/25jrika2/25jrika2a_22201_m_02.php") == 0,
                "U01 exact production voltage URL");
        Require(std::strcmp(MHQ::SelectHelpUrl(MHQ::MeterHelpKind::Current), MHQ::CURRENT_METER_HELP_URL) == 0,
                "U02 current selection uses current constant");
        Require(std::strcmp(MHQ::CURRENT_METER_HELP_URL,
                            "https://digi-keirin.com/js25/jrika/25jrika2/25jrika2a_21401_m_02.php") == 0,
                "U02 exact production current URL");

        const MHQ::MeterHelpKind kinds[] = {
            MHQ::MeterHelpKind::Voltage,
            MHQ::MeterHelpKind::Current,
        };
        const char* names[] = {"Voltage", "Current"};
        for (std::size_t index = 0; index < 2U; ++index)
        {
            const char* url = MHQ::SelectHelpUrl(kinds[index]);
            const MHQ::HelpQrGeometry evaluated = MHQ::EvaluateHelpQrGeometry(url);
            MHQ::HelpQrGeometry built;
            std::vector<std::vector<bool>> bitmap;
            Require(MHQ::ValidateHelpUrl(url), "U03 production URL validates");
            Require(MHQ::BuildHelpQrBitmap(bitmap, url, built), "U03 production URL encodes");
            Require(evaluated.valid && built.valid, "U03 production geometry is valid");
            Require(evaluated.moduleCount == built.moduleCount && evaluated.moduleScale == built.moduleScale &&
                        evaluated.renderPixels == built.renderPixels,
                    "U03 evaluation and rendered bitmap geometry agree");
            Require(built.moduleScale >= 4, "U03 production module scale is at least four");
            Require(built.moduleCount == 33 && built.moduleScale == 4 && built.renderPixels == 164,
                    "U03 exact bundled production geometry is frozen");
            Require(built.renderPixels <= MHQ::kHelpQrAreaPixels, "U03 quiet-zone render fits 185 square");
            Require(bitmap.size() == static_cast<std::size_t>(built.moduleCount), "U03 bitmap module count is exact");
            std::cout << "U03 " << names[index] << " url_bytes=" << std::strlen(url) << " modules=" << built.moduleCount
                      << " area=" << MHQ::kHelpQrAreaPixels << 'x' << MHQ::kHelpQrAreaPixels
                      << " quiet_zone_modules=" << MHQ::kHelpQrQuietZoneModules << " module_scale=" << built.moduleScale
                      << " render_pixels=" << built.renderPixels << " PASS\n";
        }

        Require(!MHQ::ValidateHelpUrl(nullptr), "empty null URL rejected");
        Require(!MHQ::ValidateHelpUrl(""), "empty URL rejected");
        Require(!MHQ::ValidateHelpUrl("relative/path"), "relative URL rejected");
        Require(!MHQ::ValidateHelpUrl("https:///missing-authority"), "missing authority rejected");
        Require(!MHQ::ValidateHelpUrl("https://example.com/a b"), "space rejected");
        Require(!MHQ::ValidateHelpUrl("https://example.com/a\nb"), "control character rejected");
        std::vector<char> oversized(MHQ::kQrEncodeGuaranteedMaxBytes + 10U, 'a');
        const char prefix[] = "https://example.com/";
        std::memcpy(&oversized[0], prefix, sizeof(prefix) - 1U);
        oversized.back() = '\0';
        Require(!MHQ::ValidateHelpUrl(&oversized[0]), "over-guarantee URL rejected");
    }

    void TestHelpInputAndNoNetworkEffects()
    {
        MHQ::MeterHelpInteraction interaction;
        Require(MHQ::ResolveHelpInput(false, false) == MHQ::MeterHelpAction::None, "U04 Help idle has no effect");
        Require(MHQ::ResolveHelpInput(false, true) == MHQ::MeterHelpAction::None, "U06 encoder is not Back");
        Require(MHQ::ResolveHelpInput(true, false) == MHQ::MeterHelpAction::Return, "U05 side returns to originating context");
        Require(MHQ::ResolveHelpInput(true, true) == MHQ::MeterHelpAction::Return,
                "U05 Side remains the only Help transition when both inputs click");
        Require(!interaction.returnPending(), "U04 Help initially owns normal input");
        interaction.requestReturn();
        Require(interaction.returnPending(), "U05 Side dismissal enters release barrier");
        Require(!interaction.canReleaseOwnership(false, true), "U06 Side dismissal while Encoder is held keeps Help ownership");
        Require(!interaction.canReleaseOwnership(true, false), "U05 held Side keeps Help ownership");
        Require(interaction.canReleaseOwnership(false, false), "U04 Help releases only after both inputs are released");
        interaction.reset();
        Require(!interaction.returnPending(), "U04 return restores originating context without stale Help ownership");
    }

    void TestWaveformInputRouting()
    {
        const LSS::State inactive = LSS::State::Inactive;
        Require(LSC::ResolveWaveformInput(SideClick(), LSC::RecorderActivity::Idle, inactive, true) ==
                    LSC::ForegroundAction::StartLiveView,
                "U07 idle Side requests Live View");
        Require(LSC::ResolveWaveformInput(EncoderClick(), LSC::RecorderActivity::Idle, inactive, true) ==
                    LSC::ForegroundAction::StartRecording,
                "U08 idle Encoder requests CSV");

        LSC::InputSnapshot simultaneous;
        simultaneous.sideClicked = true;
        simultaneous.encoderClicked = true;
        Require(LSC::ResolveWaveformInput(simultaneous, LSC::RecorderActivity::Idle, inactive, true) ==
                    LSC::ForegroundAction::None,
                "simultaneous start edges reject both");

        const LSC::RecorderActivity busyRecorder[] = {
            LSC::RecorderActivity::WaitingTrigger,
            LSC::RecorderActivity::Recording,
            LSC::RecorderActivity::Saving,
        };
        for (std::size_t index = 0; index < 3U; ++index)
        {
            Require(!LSC::CanStartLiveView(busyRecorder[index], inactive), "U09-U11 busy recorder blocks Live View guard");
            Require(LSC::ResolveWaveformInput(SideClick(), busyRecorder[index], inactive, true) == LSC::ForegroundAction::None,
                    "U09-U11 busy recorder blocks Live View route");
        }
        Require(!LSC::CanStartLiveView(LSC::RecorderActivity::BusyCleanup, inactive), "destroy timeout debt blocks Live View");
        Require(LSC::ResolveWaveformInput(EncoderClick(), LSC::RecorderActivity::BusyCleanup, inactive, true) ==
                    LSC::ForegroundAction::RetryRecorderCleanup,
                "destroy timeout click retries cleanup without starting CSV");

        const LSS::State liveOwned[] = {
            LSS::State::Starting,
            LSS::State::WifiQr,
            LSS::State::ViewerQr,
            LSS::State::Stopping,
            LSS::State::StopRecovery,
            LSS::State::StartError,
        };
        for (std::size_t index = 0; index < 6U; ++index)
        {
            Require(!LSC::CanStartRecording(LSC::RecorderActivity::Idle, liveOwned[index]),
                    "U12-U17 Live-owned state blocks CSV guard");
            Require(LSC::ResolveWaveformInput(EncoderClick(), LSC::RecorderActivity::Idle, liveOwned[index], true) ==
                        LSC::ForegroundAction::None,
                    "U12-U17 Live-owned state blocks CSV route");
        }

        const LSC::RecorderActivity activities[] = {
            LSC::RecorderActivity::Idle,
            LSC::RecorderActivity::WaitingTrigger,
            LSC::RecorderActivity::Recording,
            LSC::RecorderActivity::Saving,
            LSC::RecorderActivity::BusyCleanup,
        };
        const LSS::State states[] = {
            LSS::State::Inactive,
            LSS::State::Starting,
            LSS::State::WifiQr,
            LSS::State::ViewerQr,
            LSS::State::Stopping,
            LSS::State::StopRecovery,
            LSS::State::StartError,
        };
        for (std::size_t a = 0; a < 5U; ++a)
        {
            for (std::size_t s = 0; s < 7U; ++s)
            {
                const bool liveAllowed = LSC::CanStartLiveView(activities[a], states[s]);
                const bool csvAllowed = LSC::CanStartRecording(activities[a], states[s]);
                Require(liveAllowed == (a == 0U && s == 0U), "two-layer Live guard exhaustive legality");
                Require(csvAllowed == (a == 0U && s == 0U), "two-layer CSV guard exhaustive legality");
            }
        }
    }

    void TestTypedStartResultsAndDismiss()
    {
        const WSO::StartResult results[] = {
            WSO::StartResult::Started,
            WSO::StartResult::BusyOtherOwner,
            WSO::StartResult::ApStartFailed,
            WSO::StartResult::RetainedApNeedsStopRetry,
            WSO::StartResult::RetainedServerNeedsStopRetry,
            WSO::StartResult::AllocationOrListenFailure,
            WSO::StartResult::RouteOrRegistrationFailure,
        };
        const LSS::StartOutcome outcomes[] = {
            LSS::StartOutcome::Started,
            LSS::StartOutcome::BusyOtherOwner,
            LSS::StartOutcome::ApStartFailed,
            LSS::StartOutcome::RetainedApNeedsStopRetry,
            LSS::StartOutcome::RetainedServerNeedsStopRetry,
            LSS::StartOutcome::AllocationOrListenFailure,
            LSS::StartOutcome::RouteOrRegistrationFailure,
        };
        for (std::size_t index = 0; index < 7U; ++index)
        {
            Require(LSC::MapStartResult(results[index]) == outcomes[index], "typed StartResult mapping is lossless");
            Require(std::strcmp(LSS::StartOutcomeName(outcomes[index]), "Unknown") != 0, "typed diagnostic name is retained");
        }

        const WSO::StartResult cleanFailures[] = {
            WSO::StartResult::BusyOtherOwner,
            WSO::StartResult::ApStartFailed,
            WSO::StartResult::AllocationOrListenFailure,
            WSO::StartResult::RouteOrRegistrationFailure,
        };
        for (std::size_t index = 0; index < 4U; ++index)
        {
            FakeTransport fake;
            fake.startResult = cleanFailures[index];
            LSC::LiveShareController controller(fake.callbacks());
            Require(controller.requestStart(LSC::RecorderActivity::Idle), "U18-U21 clean start request accepted");
            Require(controller.state() == LSS::State::StartError, "U18-U21 clean failure presents StartError");
            Require(fake.finishCalls == 0, "U18-U21 clean and busy failures perform zero System cleanup");
            Require(!controller.requestStart(LSC::RecorderActivity::Idle), "U24 StartError cannot directly requestStart");
            controller.update(SideClick());
            Require(controller.isInactive(), "U25 side dismiss returns inactive");
            Require(fake.startCalls == 1, "U25 dismiss click does not also start network");
            fake.startResult = WSO::StartResult::Started;
            Require(controller.requestStart(LSC::RecorderActivity::Idle), "U29 later start works after valid dismiss");
            Require(controller.state() == LSS::State::WifiQr, "U29 failed start does not permanently lock");
        }
    }

    void TestRetainedDebtAndRecovery()
    {
        const WSO::StartResult retained[] = {
            WSO::StartResult::RetainedApNeedsStopRetry,
            WSO::StartResult::RetainedServerNeedsStopRetry,
        };
        for (std::size_t index = 0; index < 2U; ++index)
        {
            FakeTransport fake;
            fake.startResult = retained[index];
            fake.stopResult = WSO::StopResult::RetryRequired;
            LSC::LiveShareController controller(fake.callbacks());
            Require(controller.requestStart(LSC::RecorderActivity::Idle), "U22-U23 retained start request accepted");
            Require(fake.finishCalls == 1, "U22-U23 retained debt invokes typed System cleanup");
            Require(controller.state() == LSS::State::StopRecovery, "U26 retained cleanup failure remains recovery locked");
            Require(!LSC::CanStartRecording(LSC::RecorderActivity::Idle, controller.state()),
                    "U17 retained recovery blocks CSV");
            Require(!controller.requestStart(LSC::RecorderActivity::Idle), "U28 cleanup failure blocks Live retry");
            fake.stopResult = WSO::StopResult::AlreadyStopped;
            controller.update(SideClick());
            Require(fake.finishCalls == 2, "U27 recovery retry calls cleanup once");
            Require(controller.isInactive(), "U27 AlreadyStopped restores availability");
        }
    }

    void TestLifecycleAndNoStaleOwner()
    {
        FakeTransport fake;
        LSC::LiveShareController controller(fake.callbacks());
        for (unsigned cycle = 0; cycle < 32U; ++cycle)
        {
            Require(controller.requestStart(LSC::RecorderActivity::Idle), "U30 cycle start accepted");
            Require(controller.state() == LSS::State::WifiQr, "U30 cycle owns Wi-Fi QR");
            controller.update(SideClick());
            Require(controller.isInactive(), "U30 cycle stop releases interaction owner");
        }
        Require(fake.startCalls == 32 && fake.beginCalls == 32 && fake.finishCalls == 32,
                "U30 repeated cycles have balanced lifecycle calls");

        FakeTransport cleanFailure;
        cleanFailure.startResult = WSO::StartResult::RouteOrRegistrationFailure;
        LSC::LiveShareController cleanController(cleanFailure.callbacks());
        for (unsigned cycle = 0; cycle < 16U; ++cycle)
        {
            Require(cleanController.requestStart(LSC::RecorderActivity::Idle), "U30 clean error cycle start accepted");
            Require(cleanController.state() == LSS::State::StartError, "U30 clean error owns presentation");
            cleanController.update(SideClick());
            Require(cleanController.isInactive(), "U30 clean error dismiss releases owner");
        }
        Require(cleanFailure.startCalls == 16 && cleanFailure.finishCalls == 0,
                "U30 clean error cycles have no stale owner or cleanup call");

        FakeTransport retained;
        retained.startResult = WSO::StartResult::RetainedServerNeedsStopRetry;
        retained.stopResult = WSO::StopResult::RetryRequired;
        LSC::LiveShareController retainedController(retained.callbacks());
        for (unsigned cycle = 0; cycle < 16U; ++cycle)
        {
            Require(retainedController.requestStart(LSC::RecorderActivity::Idle), "U30 retained error cycle start accepted");
            Require(retainedController.state() == LSS::State::StopRecovery, "U30 retained error stays locked");
            retained.stopResult = WSO::StopResult::Stopped;
            retainedController.update(EncoderClick());
            Require(retainedController.isInactive(), "U30 retained cleanup releases owner");
            retained.stopResult = WSO::StopResult::RetryRequired;
        }
        Require(retained.startCalls == 16 && retained.finishCalls == 32,
                "U30 retained cycles balance initial and retry cleanup calls");
    }

    void TestSideHoldAndInvariant()
    {
        LSC::InputSnapshot held;
        held.sideHeld = true;
        const LSC::RecorderActivity exitStates[] = {
            LSC::RecorderActivity::Idle,
            LSC::RecorderActivity::WaitingTrigger,
            LSC::RecorderActivity::Recording,
            LSC::RecorderActivity::BusyCleanup,
        };
        for (std::size_t index = 0; index < 4U; ++index)
        {
            Require(LSC::ResolveWaveformInput(held, exitStates[index], LSS::State::Inactive, true) ==
                        LSC::ForegroundAction::Exit,
                    "U31 permitted Side Hold exits");
        }
        Require(LSC::ResolveWaveformInput(held, LSC::RecorderActivity::Saving, LSS::State::Inactive, true) ==
                    LSC::ForegroundAction::None,
                "U31 saving retains ignored Side Hold semantics");
        Require(LSC::ResolveWaveformInput(held, LSC::RecorderActivity::Idle, LSS::State::WifiQr, true) ==
                    LSC::ForegroundAction::None,
                "Live Share owns Side input while active");

        FakeTransport fake;
        LSC::LiveShareController controller(fake.callbacks());
        Require(!controller.requestStart(LSC::RecorderActivity::WaitingTrigger), "U32 direct Live start rejects busy recorder");
        Require(fake.startCalls == 0, "U32 rejected Live start has no side effect");
        Require(controller.requestStart(LSC::RecorderActivity::Idle), "U32 legal Live start accepted");
        Require(!LSC::CanStartRecording(LSC::RecorderActivity::Idle, controller.state()),
                "U32 direct CSV guard rejects active Live Share");
    }
} // namespace

int main()
{
    TestMeterHelpUrlsAndGeometry();
    TestHelpInputAndNoNetworkEffects();
    TestWaveformInputRouting();
    TestTypedStartResultsAndDismiss();
    TestRetainedDebtAndRecovery();
    TestLifecycleAndNoStaleOwner();
    TestSideHoldAndInvariant();
    std::cout << "educational_interaction_contract_test U01-U32 PASS\n";
}
