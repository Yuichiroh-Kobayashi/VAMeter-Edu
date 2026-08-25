#include "signed_current_observation.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", message);
            std::exit(1);
        }
    }

    SIGNED_CURRENT_OBS::ObservationRecord MakeRecord(std::uint64_t sequence)
    {
        const SIGNED_CURRENT_OBS::ObservationRecord record = {
            sequence,
            1000 + sequence,
            -0.0125F,
            5.0F,
            0,
            SIGNED_CURRENT_OBS::CurrentRange::High,
            false,
            true,
            true,
        };
        return record;
    }

    void TestFormattingPreservesIndependentState()
    {
        const SIGNED_CURRENT_OBS::ObservationRecord record = MakeRecord(7);
        char output[320] = {};
        std::size_t outputLength = 0;
        Expect(SIGNED_CURRENT_OBS::FormatObservation(record, 3, output, sizeof(output), outputLength),
               "record formats into bounded output");
        const char* expected =
            "SIGNED_CURRENT_OBS v=1 seq=7 timestamp_us=1007 current_A=-1.250000019e-02 bus_V=+5.000000000e+00 "
            "valid_mask=0x00000000 range=HC current_read_ok=0 overflow_read_ok=1 overflow=1 dropped_total=3";
        Expect(std::strcmp(output, expected) == 0, "format is deterministic and machine-readable");
        Expect(outputLength == std::strlen(expected), "formatter reports exact length");
        Expect(std::strstr(output, "current_A=-1.250000019e-02") != nullptr,
               "signed negative current remains negative when invalid");
        Expect(std::strstr(output, "valid_mask=0x00000000") != nullptr, "invalid mask remains invalid");
        Expect(std::strstr(output, "current_read_ok=0 overflow_read_ok=1 overflow=1") != nullptr,
               "read and overflow states remain independent");

        SIGNED_CURRENT_OBS::ObservationRecord independent = record;
        independent.validMask = 0x02;
        independent.currentReadSucceeded = true;
        independent.overflowReadSucceeded = false;
        independent.overflowAsserted = false;
        Expect(SIGNED_CURRENT_OBS::FormatObservation(independent, 4, output, sizeof(output), outputLength),
               "independent state combination formats");
        Expect(std::strstr(output, "valid_mask=0x00000002") != nullptr, "valid mask is not derived by formatter");
        Expect(std::strstr(output, "current_read_ok=1 overflow_read_ok=0 overflow=0") != nullptr,
               "formatter does not couple read and overflow states");
    }

    void TestRangeTokens()
    {
        Expect(std::strcmp(SIGNED_CURRENT_OBS::CurrentRangeToken(SIGNED_CURRENT_OBS::CurrentRange::Low), "LC") == 0,
               "low-current range token is deterministic");
        Expect(std::strcmp(SIGNED_CURRENT_OBS::CurrentRangeToken(SIGNED_CURRENT_OBS::CurrentRange::High), "HC") == 0,
               "high-current range token is deterministic");
    }

    void TestBoundedDropWithoutOverwrite()
    {
        SIGNED_CURRENT_OBS::ObservationRing<3> ring;
        Expect(ring.tryPush(MakeRecord(10)), "first record accepted");
        Expect(ring.tryPush(MakeRecord(11)), "second record accepted");
        Expect(ring.tryPush(MakeRecord(12)), "third record accepted at capacity");
        Expect(ring.queuedCount() == 3, "queue reaches fixed capacity");

        Expect(!ring.tryPush(MakeRecord(13)), "full queue rejects without waiting");
        Expect(ring.queuedCount() == 3, "rejected record does not change queue occupancy");
        Expect(ring.droppedRecordCount() == 1, "OBS-specific drop counter increments");

        SIGNED_CURRENT_OBS::ObservationRecord popped = {};
        Expect(ring.tryPop(popped) && popped.sequence == 10, "full queue does not overwrite oldest record");
        Expect(ring.tryPop(popped) && popped.sequence == 11, "second retained record remains ordered");
        Expect(ring.tryPop(popped) && popped.sequence == 12, "third retained record remains ordered");
        Expect(!ring.tryPop(popped), "empty queue reports no record");

        Expect(ring.tryPush(MakeRecord(14)), "queue accepts again after capacity is released");
        Expect(ring.tryPop(popped) && popped.sequence == 14, "new record is not ambiguous with rejected sequence");
        Expect(ring.droppedRecordCount() == 1, "drop counter remains cumulative");
    }

    void TestBoundedFormatterFailure()
    {
        char output[16] = {};
        std::size_t outputLength = 99;
        Expect(!SIGNED_CURRENT_OBS::FormatObservation(MakeRecord(1), 0, output, sizeof(output), outputLength),
               "undersized output buffer fails closed");
        Expect(outputLength == 0, "failed formatter exposes no partial length");
        Expect(output[sizeof(output) - 1] == '\0', "failed formatter remains terminated");
    }
} // namespace

int main()
{
    TestFormattingPreservesIndependentState();
    TestRangeTokens();
    TestBoundedDropWithoutOverwrite();
    TestBoundedFormatterFailure();
    std::puts("signed_current_observation_test: PASS");
    return 0;
}
