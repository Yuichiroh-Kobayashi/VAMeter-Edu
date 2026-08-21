#include "live_share_safety.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace LSS = LIVE_SHARE_SAFETY;

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

    struct FakeLifecycle
    {
        bool relayOn;
        bool producerActive;
        bool admissionReady;
        bool apActive;
        bool apStopSucceeds;
        LSS::HttpdStopDisposition httpdDisposition;
        unsigned producerRestarts;
        unsigned admissionEnables;
        unsigned ownerResurrections;
        std::vector<LSS::Action> trace;

        static bool RelayOff(void* context)
        {
            FakeLifecycle* fake = static_cast<FakeLifecycle*>(context);
            fake->trace.push_back(LSS::Action::RelayOff);
            fake->relayOn = false;
            return !fake->relayOn;
        }

        static bool ProducerInvalidate(void* context)
        {
            FakeLifecycle* fake = static_cast<FakeLifecycle*>(context);
            fake->trace.push_back(LSS::Action::ProducerInvalidate);
            fake->producerActive = false;
            return !fake->producerActive;
        }

        static bool BestEffortTransportTermination(void* context)
        {
            FakeLifecycle* fake = static_cast<FakeLifecycle*>(context);
            fake->trace.push_back(LSS::Action::BestEffortTransportTermination);
            return true;
        }

        static bool AdmissionOff(void* context)
        {
            FakeLifecycle* fake = static_cast<FakeLifecycle*>(context);
            fake->trace.push_back(LSS::Action::AdmissionOff);
            fake->admissionReady = false;
            return !fake->admissionReady;
        }

        static LSS::HttpdStopDisposition HttpdStop(void* context)
        {
            FakeLifecycle* fake = static_cast<FakeLifecycle*>(context);
            fake->trace.push_back(LSS::Action::HttpdStop);
            return fake->httpdDisposition;
        }

        static bool ApStop(void* context)
        {
            FakeLifecycle* fake = static_cast<FakeLifecycle*>(context);
            fake->trace.push_back(LSS::Action::ApStop);
            if (fake->apStopSucceeds)
                fake->apActive = false;
            return !fake->apActive;
        }
    };

    FakeLifecycle MakeFake(bool relayOn,
                           LSS::HttpdStopDisposition disposition = LSS::HttpdStopDisposition::Stopped,
                           bool apActive = true,
                           bool apStopSucceeds = true)
    {
        FakeLifecycle fake = {
            relayOn,
            true,
            true,
            apActive,
            apStopSucceeds,
            disposition,
            0,
            0,
            0,
            std::vector<LSS::Action>(),
        };
        return fake;
    }

    LSS::Callbacks MakeCallbacks(FakeLifecycle& fake)
    {
        const LSS::Callbacks callbacks = {
            &fake,
            FakeLifecycle::RelayOff,
            FakeLifecycle::ProducerInvalidate,
            FakeLifecycle::BestEffortTransportTermination,
            FakeLifecycle::AdmissionOff,
            FakeLifecycle::HttpdStop,
            FakeLifecycle::ApStop,
        };
        return callbacks;
    }

    void ExpectTrace(const FakeLifecycle& fake, bool expectApStop, const char* label)
    {
        const LSS::Action prefix[] = {
            LSS::Action::RelayOff,
            LSS::Action::ProducerInvalidate,
            LSS::Action::BestEffortTransportTermination,
            LSS::Action::AdmissionOff,
            LSS::Action::HttpdStop,
        };
        Require(fake.trace.size() == (expectApStop ? 6U : 5U), label);
        for (std::size_t index = 0; index < sizeof(prefix) / sizeof(prefix[0]); ++index)
            Require(fake.trace[index] == prefix[index], label);
        if (expectApStop)
            Require(fake.trace[5] == LSS::Action::ApStop, label);
    }

    LSS::Result Run(FakeLifecycle& fake, LSS::TerminationReason reason)
    {
        fake.trace.clear();
        return LSS::Execute(reason, MakeCallbacks(fake));
    }

    void ExpectFailClosed(const FakeLifecycle& fake, const char* label)
    {
        Require(!fake.relayOn && !fake.producerActive && !fake.admissionReady, label);
        Require(fake.producerRestarts == 0 && fake.admissionEnables == 0 && fake.ownerResurrections == 0, label);
    }

    void TestNormalAndFault()
    {
        FakeLifecycle normal = MakeFake(true);
        const LSS::Result exit = Run(normal, LSS::TerminationReason::MeasurementExit);
        ExpectTrace(normal, true, "normal exit trace");
        ExpectFailClosed(normal, "normal exit fail closed");
        Require(exit.teardownComplete(), "normal exit complete");

        FakeLifecycle fault = MakeFake(true);
        const LSS::Result result = Run(fault, LSS::TerminationReason::MeasurementFault);
        ExpectTrace(fault, true, "measurement fault trace");
        ExpectFailClosed(fault, "measurement fault fail closed");
        Require(result.teardownComplete(), "measurement fault complete");
    }

    void TestInactiveAndAlreadyStopped()
    {
        FakeLifecycle inactive = MakeFake(false, LSS::HttpdStopDisposition::AlreadyStopped, false);
        inactive.producerActive = false;
        inactive.admissionReady = false;
        const LSS::Result noLive = Run(inactive, LSS::TerminationReason::MeasurementExit);
        ExpectTrace(inactive, true, "no active SystemLive trace");
        ExpectFailClosed(inactive, "no active SystemLive fail closed");
        Require(noLive.teardownComplete(), "no active SystemLive complete");

        FakeLifecycle alreadyStopped = MakeFake(true, LSS::HttpdStopDisposition::AlreadyStopped, true);
        const LSS::Result result = Run(alreadyStopped, LSS::TerminationReason::MeasurementFault);
        ExpectTrace(alreadyStopped, true, "already stopped HTTPD trace");
        Require(result.teardownComplete() && !alreadyStopped.apActive, "already stopped HTTPD stops AP");
    }

    void TestHttpdDebtAndApFailure()
    {
        FakeLifecycle retry = MakeFake(true, LSS::HttpdStopDisposition::RetryRequired);
        const LSS::Result retryResult = Run(retry, LSS::TerminationReason::MeasurementExit);
        ExpectTrace(retry, false, "HTTPD retry trace");
        ExpectFailClosed(retry, "HTTPD retry fail closed");
        Require(retry.apActive && retryResult.hasCleanupDebt() && !retryResult.apStopAttempted,
                "HTTPD retry retains AP debt");

        FakeLifecycle wrongOwner = MakeFake(true, LSS::HttpdStopDisposition::RejectedWrongOwner);
        const LSS::Result wrongResult = Run(wrongOwner, LSS::TerminationReason::MeasurementFault);
        ExpectTrace(wrongOwner, false, "wrong owner trace");
        ExpectFailClosed(wrongOwner, "wrong owner fail closed");
        Require(wrongOwner.apActive && wrongResult.hasCleanupDebt() && !wrongResult.apStopAttempted,
                "wrong owner retains AP");

        FakeLifecycle apFailure = MakeFake(true, LSS::HttpdStopDisposition::Stopped, true, false);
        const LSS::Result apResult = Run(apFailure, LSS::TerminationReason::MeasurementExit);
        ExpectTrace(apFailure, true, "AP failure trace");
        ExpectFailClosed(apFailure, "AP failure fail closed");
        Require(apFailure.apActive && apResult.hasCleanupDebt() && apResult.apStopAttempted && !apResult.apStopped,
                "AP failure debt");
    }

    void TestIdempotence()
    {
        FakeLifecycle repeatedExit = MakeFake(true);
        Run(repeatedExit, LSS::TerminationReason::MeasurementExit);
        const LSS::Result exitAgain = Run(repeatedExit, LSS::TerminationReason::MeasurementExit);
        ExpectTrace(repeatedExit, true, "repeated exit trace");
        ExpectFailClosed(repeatedExit, "repeated exit fail closed");
        Require(exitAgain.teardownComplete(), "repeated exit complete");

        FakeLifecycle repeatedFault = MakeFake(true);
        Run(repeatedFault, LSS::TerminationReason::MeasurementFault);
        const LSS::Result faultAgain = Run(repeatedFault, LSS::TerminationReason::MeasurementFault);
        ExpectTrace(repeatedFault, true, "repeated fault trace");
        ExpectFailClosed(repeatedFault, "repeated fault fail closed");
        Require(faultAgain.teardownComplete(), "repeated fault complete");

        FakeLifecycle faultThenExit = MakeFake(true);
        Run(faultThenExit, LSS::TerminationReason::MeasurementFault);
        const LSS::Result exit = Run(faultThenExit, LSS::TerminationReason::MeasurementExit);
        ExpectTrace(faultThenExit, true, "fault then exit trace");
        ExpectFailClosed(faultThenExit, "fault then exit fail closed");
        Require(exit.teardownComplete(), "fault then exit complete");
    }

    void TestPartialStartupAndRelayStates()
    {
        FakeLifecycle partial = MakeFake(true);
        partial.producerActive = false;
        partial.admissionReady = false;
        const LSS::Result result = Run(partial, LSS::TerminationReason::MeasurementFault);
        ExpectTrace(partial, true, "partial startup trace");
        ExpectFailClosed(partial, "partial startup fail closed");
        Require(result.teardownComplete(), "partial startup complete");

        FakeLifecycle initiallyOn = MakeFake(true);
        Run(initiallyOn, LSS::TerminationReason::MeasurementExit);
        ExpectTrace(initiallyOn, true, "relay initially ON trace");
        Require(!initiallyOn.relayOn, "relay initially ON becomes OFF");

        FakeLifecycle initiallyOff = MakeFake(false);
        Run(initiallyOff, LSS::TerminationReason::MeasurementFault);
        ExpectTrace(initiallyOff, true, "relay initially OFF trace");
        Require(!initiallyOff.relayOn, "relay initially OFF remains OFF");
    }

    void TestStopSharingPolicy()
    {
        FakeLifecycle fake = MakeFake(true);
        const LSS::Result result = Run(fake, LSS::TerminationReason::StopSharing);
        Require(!result.executable && fake.trace.empty(), "StopSharing is not executable in C2A");
        Require(fake.relayOn, "StopSharing does not imply RelayOff");
    }
} // namespace

int main()
{
    TestNormalAndFault();
    TestInactiveAndAlreadyStopped();
    TestHttpdDebtAndApFailure();
    TestIdempotence();
    TestPartialStartupAndRelayStates();
    TestStopSharingPolicy();
    std::cout << "live_share_safety_test PASS\n";
    return 0;
}
