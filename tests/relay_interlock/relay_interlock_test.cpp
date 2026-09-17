#include "relay_interlock.h"

#include <cstdlib>
#include <iostream>

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
    RELAY_INTERLOCK::RelayInterlock interlock;
    Expect(!interlock.isInitialized(), "policy starts uninitialized");
    Expect(!interlock.requestNormal(true).apply, "pre-init ON is denied");
    const RELAY_INTERLOCK::Command preInitOff = interlock.requestNormal(false);
    Expect(preInitOff.apply && !preInitOff.state, "pre-init OFF remains allowed");
    Expect(!interlock.commitFault().apply && !interlock.isFaultLatched(),
           "pre-init fault cannot operate or latch the relay policy");

    interlock.initialize();
    Expect(interlock.isInitialized(), "policy initializes after raw boot OFF");
    const RELAY_INTERLOCK::Command normalOn = interlock.requestNormal(true);
    Expect(normalOn.apply && normalOn.state, "ON is allowed after initialization before fault");

    const RELAY_INTERLOCK::Command faultOff = interlock.commitFault();
    Expect(interlock.isFaultLatched() && faultOff.apply && !faultOff.state,
           "fault latch commits before the one-shot OFF action is returned");
    Expect(!interlock.requestNormal(true).apply, "ON is denied after latch");
    Expect(!interlock.commitFault().apply, "fault does not request OFF twice or auto-reclose");
    const RELAY_INTERLOCK::Command offOne = interlock.requestNormal(false);
    const RELAY_INTERLOCK::Command offTwo = interlock.requestNormal(false);
    Expect(offOne.apply && !offOne.state && offTwo.apply && !offTwo.state,
           "normal OFF remains allowed and idempotent after latch");

    RELAY_INTERLOCK::RelayInterlock reconstructed;
    Expect(!reconstructed.isInitialized() && !reconstructed.isFaultLatched(),
           "object reconstruction is the only pure-model reset");

    std::cout << "PASS: relay interlock policy\n";
}
