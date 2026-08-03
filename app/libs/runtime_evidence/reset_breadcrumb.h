#pragma once

#include <cstddef>
#include <cstdint>

namespace D2B_RESET_BREADCRUMB
{
    // The breadcrumb is deliberately a fixed-width record.  Keep additions as
    // uint32_t fields and update CanonicalChecksum when the format changes;
    // never checksum compiler padding.
    static const std::uint32_t kMagic = 0x44324252U; // "D2BR"
    static const std::uint32_t kFormatVersion = 1U;
    static const std::uint32_t kValidMarker = 0xD2B0C0DEU;
    static const std::uint32_t kInvalidMarker = 0U;
    static const std::uint32_t kUnmeasuredStack = 0xFFFFFFFFU;

    enum class Stage : std::uint32_t
    {
        NONE = 0U,
        BOOT_REPORTED = 10U,
        SERVER_STARTED = 20U,
        WS_HANDLER_GET_AFTER_101_ENTER = 100U,
        ORIGIN_CHECK_BEGIN = 110U,
        ORIGIN_CHECK_OK = 111U,
        OWNER_PUBLISHED = 120U,
        PIPELINE_OPEN_ENTER = 130U,
        PIPELINE_OPEN_OK = 131U,
        CONNECT_EVIDENCE_ENTER = 140U,
        CONNECT_EVIDENCE_OK = 141U,
        OPEN_HANDLER_RETURN = 150U,
        WS_FRAME_HANDLER_ENTER = 200U,
        FRAME_HEADER_READ_OK = 210U,
        FRAME_PAYLOAD_READ_OK = 220U,
        REASSEMBLY_COMPLETE = 230U,
        CONTROL_PARSE_ENTER = 240U,
        CONTROL_PARSE_OK = 241U,
        HELLO_RECOGNIZED = 245U,
        CONTROL_HANDLE_ENTER = 250U,
        WELCOME_BUILT = 251U,
        WELCOME_SEND_ENTER = 260U,
        WELCOME_SEND_OK = 261U,
        SESSION_READY_COMMIT = 270U,
        WS_RECEIVE_RETURN = 280U,
        WS_CLOSE_ENTER = 300U,
        WS_CLOSE_COMPLETE = 301U,
        NORMAL_LIFECYCLE_COMPLETE = 900U,
    };

    // All fields are explicitly four bytes.  The layout is not the wire or
    // checksum format; CanonicalChecksum serializes each field in its listed
    // order and little-endian representation.
    struct Record
    {
        std::uint32_t magic;
        std::uint32_t format_version;
        std::uint32_t sequence;
        std::uint32_t valid_marker;
        std::uint32_t stage;
        std::uint32_t stage_inverse;
        std::uint32_t server_generation;
        std::uint32_t websocket_generation;
        std::int32_t socket;
        std::uint32_t stream_id;
        std::uint32_t configured_httpd_stack_bytes;
        std::uint32_t httpd_stack_high_water_raw;
        std::uint32_t httpd_stack_high_water_bytes;
        std::uint32_t internal_heap_free;
        std::uint32_t internal_heap_min;
        std::uint32_t internal_heap_largest;
        std::uint32_t reset_reason_code;
        std::uint32_t reset_reason_raw;
        std::uint32_t httpd_stack_sample_valid;
        std::uint32_t checksum;
    };

    static_assert(sizeof(Record) == 20U * sizeof(std::uint32_t), "breadcrumb record must stay fixed-width");

    const char* StageName(std::uint32_t stage);
    const char* StageName(Stage stage);
    bool IsKnownStage(std::uint32_t stage);

    Record EmptyRecord();
    Record MakeRecord(Stage stage,
                      std::uint32_t sequence,
                      std::uint32_t serverGeneration,
                      std::uint32_t websocketGeneration,
                      std::int32_t socket,
                      std::uint32_t streamId,
                      std::uint32_t configuredStackBytes,
                      std::uint32_t stackRaw,
                      std::uint32_t stackBytes,
                      std::uint32_t stackSampleValid,
                      std::uint32_t heapFree,
                      std::uint32_t heapMin,
                      std::uint32_t heapLargest,
                      std::uint32_t resetReasonCode = 0U,
                      std::uint32_t resetReasonRaw = 0U);

    // Validation is safe on arbitrary/random memory and accepts unknown stage
    // IDs so future firmware can still preserve the last boundary.
    bool IsValid(const Record& record);
    std::uint32_t CanonicalChecksum(const Record& record);

    // Commit helpers are used by the RTC adapter and by host torn-write tests.
    // They intentionally write the validity marker first/last and copy every
    // body field explicitly.
    void Invalidate(Record& target);
    void Commit(Record& target, const Record& source);

    std::uint32_t NextSequence(std::uint32_t current, bool haveCurrent);
    bool IsSequenceNewer(std::uint32_t candidate, std::uint32_t incumbent);
    unsigned SelectNewestSlot(const Record& slot0, const Record& slot1, bool& haveValid);

    // Capture and replay are pure helpers shared by the RTC adapter and host
    // tests.  Only a fully valid newest slot is captured; no-prior formatting
    // is deterministic, bounded, and NUL-safe.
    bool CapturePriorSnapshot(const Record& slot0, const Record& slot1, Record& prior);
    std::size_t FormatCapturedPriorSnapshot(const Record& prior,
                                            bool havePrior,
                                            char* buffer,
                                            std::size_t capacity);
    std::size_t FormatPriorSnapshot(const Record& slot0,
                                    const Record& slot1,
                                    char* buffer,
                                    std::size_t capacity);

    /*
     * FormatRecord uses only the caller-owned buffer.  It returns the required
     * line length excluding NUL even when truncated; a non-empty buffer is
     * always NUL terminated.  The text contains no user/network data.
     */
    std::size_t FormatRecord(const Record& record, char* buffer, std::size_t capacity);
} // namespace D2B_RESET_BREADCRUMB
