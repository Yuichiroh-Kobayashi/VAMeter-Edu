#include "d2b_acquisition.h"

#include <cmath>
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
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Expect(D2B::BuildViValidMask(true, 3.0F, true, 0.1F, true, false) == 3,
           "both successful finite channels are valid");
    Expect(D2B::BuildViValidMask(false, 3.0F, true, 0.1F, true, false) == D2B::kCurrentValid,
           "failed bus read clears only voltage validity");
    Expect(D2B::BuildViValidMask(true, nan, true, 0.1F, true, false) == D2B::kCurrentValid,
           "non-finite voltage is invalid");
    Expect(D2B::BuildViValidMask(true, 3.0F, false, 0.1F, true, false) == D2B::kVoltageValid,
           "failed selected current read is invalid");
    Expect(D2B::BuildViValidMask(true, 3.0F, true, nan, true, false) == D2B::kVoltageValid,
           "non-finite processed current is invalid");
    Expect(D2B::BuildViValidMask(true, 3.0F, true, 0.1F, false, false) == D2B::kVoltageValid,
           "failed overflow-status read invalidates current");
    Expect(D2B::BuildViValidMask(true, 3.0F, true, 0.1F, true, true) == D2B::kVoltageValid,
           "math overflow invalidates current");

    const D2B::AcquisitionSample canonical =
        D2B::BuildAcquisitionSample(7, 1234, D2B::kVoltageValid | 0x80, 5.0F, -0.0F);
    Expect(canonical.sequence == 7 && canonical.timestampUs == 1234 &&
               canonical.validMask == D2B::kVoltageValid && canonical.voltageV == 5.0F &&
               canonical.currentA == 0.0F && !std::signbit(canonical.currentA),
           "unknown validity bits are cleared and an invalid channel is canonical positive zero");
    const D2B::AcquisitionSample nonFinite =
        D2B::BuildAcquisitionSample(8, 1235, 3, nan, nan);
    Expect(nonFinite.validMask == 0 && nonFinite.voltageV == 0.0F &&
               !std::signbit(nonFinite.voltageV) && nonFinite.currentA == 0.0F &&
               !std::signbit(nonFinite.currentA),
           "non-finite values cannot remain advertised as valid");

    D2B::AcquisitionRing ring;
    for (std::uint64_t sequence = 0; sequence < D2B::kAcquisitionQueueDepth; ++sequence)
    {
        const D2B::AcquisitionSample sample = {sequence, sequence * 10, 3, 1.0F, 2.0F};
        Expect(!ring.pushDropOldest(sample), "ring accepts samples before capacity");
    }
    Expect(ring.queuedCount() == D2B::kAcquisitionQueueDepth && ring.producerDropCount() == 0,
           "ring reaches exact depth without drop");
    const D2B::AcquisitionSample overflow = {64, 640, 3, 1.0F, 2.0F};
    Expect(ring.pushDropOldest(overflow) && ring.queuedCount() == D2B::kAcquisitionQueueDepth &&
               ring.producerDropCount() == 1,
           "full ring drops exactly one oldest sample");

    D2B::AcquisitionSample popped = {};
    bool producerOverflow = false;
    Expect(ring.pop(popped, producerOverflow) && popped.sequence == 1 && producerOverflow,
           "first retained sample exposes producer-overflow discontinuity");
    Expect(ring.pop(popped, producerOverflow) && popped.sequence == 2 && !producerOverflow,
           "producer-overflow cause is reported once");

    ring.clear();
    Expect(ring.queuedCount() == 0 && ring.producerDropCount() == 1 && !ring.pop(popped, producerOverflow),
           "clear removes queued samples but preserves aggregate drop counter");

    std::cout << "PASS: d2b V/I validity and drop-oldest acquisition ring\n";
    return 0;
}
