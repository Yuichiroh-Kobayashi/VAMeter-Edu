#include "reverse_current_safety.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }
} // namespace

int main()
{
    using REVERSE_CURRENT_SAFETY::Controller;
    using REVERSE_CURRENT_SAFETY::CurrentRange;
    const REVERSE_CURRENT_DETECTOR::Configuration testConfiguration = {-1.0F, 2};

    Controller beforeInitialization(testConfiguration);
    Expect(!beforeInitialization.observe(false, true, true, -2.0F, CurrentRange::Low).requestRelayOff,
           "no action before relay policy initialization");

    Controller disarmed(testConfiguration);
    Expect(!disarmed.observe(true, false, true, -2.0F, CurrentRange::High).requestRelayOff,
           "no action while Training arming is false");

    Controller controller(testConfiguration);
    Expect(!controller.observe(true, true, true, -2.0F, CurrentRange::Low).requestRelayOff,
           "CURRENT_BEHAVIOR / P1-BY-OMISSION: candidate does not request OFF");
    const REVERSE_CURRENT_SAFETY::Action fault = controller.observe(true, true, true, -2.0F, CurrentRange::High);
    Expect(fault.faultLatched && controller.isFaultLatched() && fault.requestRelayOff,
           "range switching has no special production policy; latch commits before OFF request");
    Expect(!controller.authorizeRelayOn(), "ON denied while latched");
    Expect(controller.authorizeRelayOff() && controller.authorizeRelayOff(), "OFF remains allowed and idempotent");
    Expect(!controller.observe(true, true, true, 0.0F, CurrentRange::Low).requestRelayOff, "zero neither clears nor retries");
    Expect(!controller.observe(true, true, true, 2.0F, CurrentRange::Low).requestRelayOff,
           "positive input neither clears nor retries");
    Expect(!controller.observe(true, true, false, -2.0F, CurrentRange::Low).requestRelayOff,
           "invalid input neither clears nor retries");
    Expect(controller.isFaultLatched(), "fault remains latched");
    Expect(!controller.observe(true, true, true, -2.0F, CurrentRange::Low).requestRelayOff, "no automatic retry or reclose");

    Controller reconstructed(testConfiguration);
    Expect(!reconstructed.isFaultLatched() && reconstructed.authorizeRelayOn(),
           "object reconstruction is the only pure-model clear");
    Expect(reconstructed.authorizeRelayOff(), "OFF remains allowed before a fault");

    const REVERSE_CURRENT_DETECTOR::Configuration disabled = REVERSE_CURRENT_DETECTOR::ProductionDisabledConfiguration();
    REVERSE_CURRENT_DETECTOR::Detector productionDetector(disabled);
    Expect(!productionDetector.isConfigurationValid(), "B0 production detector is invalid by construction");
    Controller production(disabled);
    for (int i = 0; i != 8; ++i)
        Expect(!production.observe(true, false, true, -std::numeric_limits<float>::max(), CurrentRange::High).requestRelayOff,
               "disabled detector and false arming cannot reach fault action");
    Expect(!production.isFaultLatched(), "B0 production policy remains inert");

    std::cout << "PASS: inert reverse-current safety controller\n";
}
