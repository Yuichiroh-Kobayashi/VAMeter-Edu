#include "reset_breadcrumb.h"
#include "runtime_evidence.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
    using namespace D2B_RESET_BREADCRUMB;

    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    Record Candidate(Stage stage, std::uint32_t sequence, std::uint32_t stackValid = 0U)
    {
        return MakeRecord(stage,
                          sequence,
                          12U,
                          34U,
                          -7,
                          56U,
                          4096U,
                          stackValid != 0U ? 19U : kUnmeasuredStack,
                          stackValid != 0U ? 19U : kUnmeasuredStack,
                          stackValid,
                          100U,
                          90U,
                          80U,
                          3U,
                          4U);
    }

    void CommitValid(Record& target, const Record& source)
    {
        Commit(target, source);
        Expect(IsValid(target), "committed record validates");
    }

    void CopyBodyWord(Record& target, const Record& source, unsigned word)
    {
        switch (word)
        {
        case 0:
            target.magic = source.magic;
            break;
        case 1:
            target.format_version = source.format_version;
            break;
        case 2:
            target.sequence = source.sequence;
            break;
        case 3:
            target.stage = source.stage;
            break;
        case 4:
            target.stage_inverse = source.stage_inverse;
            break;
        case 5:
            target.server_generation = source.server_generation;
            break;
        case 6:
            target.websocket_generation = source.websocket_generation;
            break;
        case 7:
            target.socket = source.socket;
            break;
        case 8:
            target.stream_id = source.stream_id;
            break;
        case 9:
            target.configured_httpd_stack_bytes = source.configured_httpd_stack_bytes;
            break;
        case 10:
            target.httpd_stack_high_water_raw = source.httpd_stack_high_water_raw;
            break;
        case 11:
            target.httpd_stack_high_water_bytes = source.httpd_stack_high_water_bytes;
            break;
        case 12:
            target.internal_heap_free = source.internal_heap_free;
            break;
        case 13:
            target.internal_heap_min = source.internal_heap_min;
            break;
        case 14:
            target.internal_heap_largest = source.internal_heap_largest;
            break;
        case 15:
            target.reset_reason_code = source.reset_reason_code;
            break;
        case 16:
            target.reset_reason_raw = source.reset_reason_raw;
            break;
        case 17:
            target.httpd_stack_sample_valid = source.httpd_stack_sample_valid;
            break;
        case 18:
            target.checksum = source.checksum;
            break;
        default:
            break;
        }
    }

    void TestValidationAndFormatting()
    {
        Record record = {};
        CommitValid(record, Candidate(Stage::NORMAL_LIFECYCLE_COMPLETE, 1U));

        Record invalid = record;
        invalid.magic ^= 1U;
        Expect(!IsValid(invalid), "invalid magic rejected");
        invalid = record;
        invalid.format_version += 1U;
        Expect(!IsValid(invalid), "invalid version rejected");
        invalid = record;
        invalid.valid_marker = kInvalidMarker;
        Expect(!IsValid(invalid), "invalid marker rejected");
        invalid = record;
        invalid.stage_inverse ^= 1U;
        Expect(!IsValid(invalid), "invalid stage inverse rejected");
        invalid = record;
        invalid.checksum ^= 1U;
        Expect(!IsValid(invalid), "invalid checksum rejected");

        Record unknown = {};
        CommitValid(unknown, Candidate(static_cast<Stage>(0xDEADU), 2U));
        Expect(IsValid(unknown), "unknown stage remains valid");
        Expect(std::strcmp(StageName(unknown.stage), "UNKNOWN") == 0, "unknown stage displays safely");

        char line[768];
        const std::size_t required = FormatRecord(record, line, sizeof(line));
        Expect(required > 0U && required < sizeof(line), "known format fits");
        Expect(std::strstr(line, "stage=900 stage_name=NORMAL_LIFECYCLE_COMPLETE") != nullptr,
               "known stage ID keeps explicit three-digit form");
        Expect(std::strstr(line, "stage_name=NORMAL_LIFECYCLE_COMPLETE") != nullptr,
               "known stage name is formatted");
        Expect(std::strstr(line, "httpd_stack_high_water_raw=4294967295") != nullptr,
               "stack sentinel is explicit");
        char shortLine[12];
        const std::size_t truncated = FormatRecord(record, shortLine, sizeof(shortLine));
        Expect(truncated == required, "truncation reports required length");
        Expect(shortLine[sizeof(shortLine) - 1U] == '\0', "truncated line remains NUL terminated");
        char empty[1] = {'x'};
        (void)FormatRecord(record, empty, 0U);

        Record random = {};
        std::uint32_t randomState = 0x13579BDFU;
        unsigned char* randomBytes = reinterpret_cast<unsigned char*>(&random);
        for (std::size_t index = 0U; index < sizeof(random); ++index)
        {
            randomState ^= randomState << 13U;
            randomState ^= randomState >> 17U;
            randomState ^= randomState << 5U;
            randomBytes[index] = static_cast<unsigned char>(randomState & 0xFFU);
        }
        random.magic ^= 1U;
        Expect(!IsValid(random), "deterministic random memory rejects safely");
    }

    void TestTornWrites()
    {
        Record oldRecord = {};
        Record newRecord = {};
        CommitValid(oldRecord, Candidate(Stage::PIPELINE_OPEN_OK, 10U));
        CommitValid(newRecord, Candidate(Stage::WS_FRAME_HANDLER_ENTER, 11U));

        Record markerOnlyInvalid = oldRecord;
        Invalidate(markerOnlyInvalid);
        Expect(!IsValid(markerOnlyInvalid), "marker invalidation with zero body writes rejects target");
        bool markerOnlyHaveValid = false;
        Expect(SelectNewestSlot(oldRecord, markerOnlyInvalid, markerOnlyHaveValid) == 0U &&
                   markerOnlyHaveValid,
               "marker-only torn boundary preserves old slot");

        for (unsigned boundary = 0U; boundary <= 18U; ++boundary)
        {
            Record torn = oldRecord;
            Invalidate(torn);
            for (unsigned word = 0U; word <= boundary; ++word)
                CopyBodyWord(torn, newRecord, word);
            Expect(!IsValid(torn), "every torn 32-bit boundary rejects target");
            bool haveValid = false;
            Expect(SelectNewestSlot(oldRecord, torn, haveValid) == 0U && haveValid,
                   "torn target leaves old slot selected");
        }

        Record final = oldRecord;
        Invalidate(final);
        for (unsigned word = 0U; word <= 18U; ++word)
            CopyBodyWord(final, newRecord, word);
        final.valid_marker = kValidMarker;
        Expect(IsValid(final), "final marker after complete body commits target");
    }

    void TestSequencesAndSlots()
    {
        Expect(NextSequence(0U, false) == 1U, "empty sequence starts at one");
        Expect(NextSequence(std::numeric_limits<std::uint32_t>::max(), true) == 1U,
               "sequence wraps max to one");
        Expect(IsSequenceNewer(1U, std::numeric_limits<std::uint32_t>::max()), "wrap sequence is newer");
        Expect(!IsSequenceNewer(0x80000001U, 1U), "half range is deterministic tie");

        Record slot0 = {};
        Record slot1 = {};
        CommitValid(slot0, Candidate(Stage::SERVER_STARTED, 22U));
        CommitValid(slot1, Candidate(Stage::WS_CLOSE_COMPLETE, 22U));
        bool haveValid = false;
        Expect(SelectNewestSlot(slot0, slot1, haveValid) == 0U && haveValid, "equal sequence picks slot zero");
        CommitValid(slot1, Candidate(Stage::WS_CLOSE_COMPLETE, 0x80000001U));
        CommitValid(slot0, Candidate(Stage::SERVER_STARTED, 1U));
        Expect(SelectNewestSlot(slot0, slot1, haveValid) == 0U, "half-range tie picks slot zero");
        Invalidate(slot1);
        Expect(SelectNewestSlot(slot0, slot1, haveValid) == 0U && haveValid, "one valid slot selected");
        Invalidate(slot0);
        Expect(SelectNewestSlot(slot0, slot1, haveValid) == 0U && !haveValid, "both invalid use slot zero tie");
    }

    void TestPriorSnapshotHelpers()
    {
        Record invalid0 = EmptyRecord();
        Record invalid1 = EmptyRecord();
        Record prior = {};
        Expect(!CapturePriorSnapshot(invalid0, invalid1, prior), "both invalid slots have no prior snapshot");

        char noPrior[128];
        const std::size_t noPriorLength = FormatPriorSnapshot(invalid0, invalid1, noPrior, sizeof(noPrior));
        Expect(std::strstr(noPrior, "prior_snapshot=none") != nullptr,
               "no-prior replay output is safe and explicit");
        char noPriorSmall[8];
        const std::size_t noPriorRequired =
            FormatCapturedPriorSnapshot(prior, false, noPriorSmall, sizeof(noPriorSmall));
        Expect(noPriorRequired == noPriorLength && noPriorSmall[sizeof(noPriorSmall) - 1U] == '\0',
               "no-prior replay truncation remains NUL-safe");

        Record slot0 = {};
        Record slot1 = {};
        CommitValid(slot0, Candidate(Stage::PIPELINE_OPEN_OK, 40U, 1U));
        CommitValid(slot1, Candidate(Stage::WS_FRAME_HANDLER_ENTER, 41U, 1U));
        Expect(CapturePriorSnapshot(slot0, slot1, prior), "newest valid slot captured as prior");
        Expect(prior.sequence == slot1.sequence && prior.stage == slot1.stage,
               "captured prior selects newest valid slot");

        char replayBefore[768];
        char replayAfter[768];
        const std::size_t replayBeforeLength =
            FormatCapturedPriorSnapshot(prior, true, replayBefore, sizeof(replayBefore));
        Record newCurrent = {};
        CommitValid(newCurrent, Candidate(Stage::BOOT_REPORTED, 42U, 0U));
        // A completed commit to the other slot creates a newer current record;
        // the already-captured prior copy must remain immutable for replay.
        Commit(slot0, newCurrent);
        Expect(IsValid(slot0), "new current slot commits independently");
        Record newestAfter = {};
        Expect(CapturePriorSnapshot(slot0, slot1, newestAfter) && newestAfter.sequence == slot0.sequence,
               "slot selection observes newer current after prior capture");
        const std::size_t replayAfterLength =
            FormatCapturedPriorSnapshot(prior, true, replayAfter, sizeof(replayAfter));
        Expect(replayBeforeLength == replayAfterLength && std::strcmp(replayBefore, replayAfter) == 0,
               "captured prior replay bytes stay unchanged after current commit");
        Expect(std::strstr(replayAfter, "stage=200") != nullptr &&
                   std::strstr(replayAfter, "sequence=41") != nullptr,
               "captured prior replay retains original fields");
    }

    void TestStackValidityAndCompletion()
    {
        Record sentinel = {};
        CommitValid(sentinel, Candidate(Stage::BOOT_REPORTED, 3U, 0U));
        Expect(sentinel.httpd_stack_sample_valid == 0U &&
                   sentinel.httpd_stack_high_water_raw == kUnmeasuredStack &&
                   sentinel.httpd_stack_high_water_bytes == kUnmeasuredStack,
               "application stack remains sentinel");
        Record sampled = {};
        CommitValid(sampled, Candidate(Stage::WS_HANDLER_GET_AFTER_101_ENTER, 4U, 1U));
        Expect(sampled.httpd_stack_sample_valid == 1U && sampled.httpd_stack_high_water_raw == 19U &&
                   sampled.httpd_stack_high_water_bytes == 19U,
               "HTTPD stack sample is valid");
        Record complete = {};
        CommitValid(complete, Candidate(Stage::NORMAL_LIFECYCLE_COMPLETE, 5U));
        Expect(complete.stage == static_cast<std::uint32_t>(Stage::NORMAL_LIFECYCLE_COMPLETE),
               "normal completion stage is persisted");

        // The UI-task replay helper is pure formatting over an immutable prior
        // copy: formatting it before/after a newer current record must remain
        // byte-for-byte identical and retain reset/stack/heap/socket evidence.
        char priorBefore[768];
        char priorAfter[768];
        const std::size_t priorLength = FormatRecord(sampled, priorBefore, sizeof(priorBefore));
        Record newer = {};
        CommitValid(newer, Candidate(Stage::WS_RECEIVE_RETURN, 6U, 1U));
        const std::size_t replayLength = FormatRecord(sampled, priorAfter, sizeof(priorAfter));
        Expect(priorLength == replayLength && std::strcmp(priorBefore, priorAfter) == 0,
               "prior snapshot replay remains unchanged after current commit");
        Expect(std::strstr(priorAfter, "socket=-7") != nullptr &&
                   std::strstr(priorAfter, "reset_reason_code=3") != nullptr,
               "replay retains socket and reset evidence");

        RUNTIME_EVIDENCE::BootResource boot = {};
        boot.reset_reason_code = 9U;
        boot.reset_reason_raw = 10U;
        boot.prior_valid = 1U;
        boot.prior_stage = sampled.stage;
        boot.prior_sequence = sampled.sequence;
        boot.prior_server_generation = sampled.server_generation;
        boot.prior_websocket_generation = sampled.websocket_generation;
        boot.prior_socket = sampled.socket;
        boot.prior_stream_id = sampled.stream_id;
        boot.prior_configured_httpd_stack_bytes = sampled.configured_httpd_stack_bytes;
        boot.prior_httpd_stack_high_water_raw = sampled.httpd_stack_high_water_raw;
        boot.prior_httpd_stack_high_water_bytes = sampled.httpd_stack_high_water_bytes;
        boot.prior_httpd_stack_sample_valid = sampled.httpd_stack_sample_valid;
        boot.prior_internal_heap_free = sampled.internal_heap_free;
        boot.prior_internal_heap_min = sampled.internal_heap_min;
        boot.prior_internal_heap_largest = sampled.internal_heap_largest;
        boot.prior_reset_reason_code = sampled.reset_reason_code;
        boot.prior_reset_reason_raw = sampled.reset_reason_raw;
        boot.prior_checksum = sampled.checksum;
        char bootLine[1024];
        (void)RUNTIME_EVIDENCE::FormatBootLine(boot, bootLine, sizeof(bootLine));
        Expect(std::strstr(bootLine, "reset_reason_code=9 reset_reason_raw=10") != nullptr,
               "boot line keeps current reset reason");
        Expect(std::strstr(bootLine, "prior_socket=-7") != nullptr &&
                   std::strstr(bootLine, "prior_httpd_stack_high_water_raw=19") != nullptr &&
                   std::strstr(bootLine, "prior_internal_heap_largest=80") != nullptr &&
                   std::strstr(bootLine, "prior_reset_reason_raw=4") != nullptr,
               "boot line keeps full prior diagnostic snapshot");
    }
} // namespace

int main()
{
    TestValidationAndFormatting();
    TestTornWrites();
    TestSequencesAndSlots();
    TestPriorSnapshotHelpers();
    TestStackValidityAndCompletion();
    std::cout << "PASS: reset breadcrumb validation, torn writes, sequence, formatter, and stack evidence\n";
    return 0;
}
