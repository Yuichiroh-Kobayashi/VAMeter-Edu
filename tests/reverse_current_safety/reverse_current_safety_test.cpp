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
           "no action or candidate advance before relay policy initialization");
    Expect(!beforeInitialization.observe(true, true, true, -2.0F, CurrentRange::Low).requestRelayOff,
           "first initialized observation remains a candidate");

    Controller trainingGate(testConfiguration);
    Expect(!trainingGate.observe(true, false, true, -2.0F, CurrentRange::High).requestRelayOff,
           "Training false does not advance the safety detector");
    Expect(!trainingGate.observe(true, true, true, -2.0F, CurrentRange::Low).requestRelayOff,
           "first armed observation is only a candidate");
    Expect(trainingGate.observe(true, true, true, -2.0F, CurrentRange::High).requestRelayOff,
           "Training test configuration reaches one-shot fault across range context");
    Expect(!trainingGate.observe(true, true, true, -2.0F, CurrentRange::High).requestRelayOff,
           "latched detector emits no automatic retry");

    Controller resetByQualifier(testConfiguration);
    Expect(!resetByQualifier.observe(true, true, true, -2.0F, CurrentRange::Low).requestRelayOff, "qualifier starts candidate");
    Expect(!resetByQualifier.observe(true, true, true, 0.0F, CurrentRange::Low).requestRelayOff,
           "valid non-qualifier resets current detector behavior");
    Expect(!resetByQualifier.observe(true, true, true, -2.0F, CurrentRange::Low).requestRelayOff,
           "qualifying count restarts after valid non-qualifier");

    Controller invalidHold(testConfiguration);
    Expect(!invalidHold.observe(true, true, true, -2.0F, CurrentRange::Low).requestRelayOff,
           "qualifier starts invalid-policy regression candidate");
    Expect(!invalidHold.observe(true, true, false, -2.0F, CurrentRange::Low).requestRelayOff,
           "CURRENT_BEHAVIOR_HOLD / NOT_PRODUCTION_SAFETY_POLICY / OWNER_DECISION_PENDING");
    Expect(!invalidHold.observe(true, true, true, std::numeric_limits<float>::quiet_NaN(), CurrentRange::Low).requestRelayOff,
           "non-finite hold is not an approved production safety policy");
    Expect(invalidHold.observe(true, true, true, -2.0F, CurrentRange::Low).requestRelayOff,
           "current hold behavior resumes candidate counting");

    const REVERSE_CURRENT_DETECTOR::Configuration disabled = REVERSE_CURRENT_DETECTOR::ProductionDisabledConfiguration();
    REVERSE_CURRENT_DETECTOR::Detector productionDetector(disabled);
    Expect(!productionDetector.isConfigurationValid(), "B0 production detector is invalid by construction");
    Controller production(disabled);
    for (int i = 0; i != 8; ++i)
        Expect(!production.observe(true, false, true, -std::numeric_limits<float>::max(), CurrentRange::High).requestRelayOff,
               "disabled detector and false Training arming cannot reach fault action");

    std::cout << "PASS: Training-gated inert reverse-current safety adapter\n";
}
