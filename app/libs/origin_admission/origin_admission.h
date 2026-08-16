#pragma once

#include <cstddef>
#include <cstdint>

namespace ORIGIN_ADMISSION
{
    static const std::size_t kMaximumOriginBytes = 191;
    static const std::uint32_t kMaximumHeaderBytes = 4096;
    static const std::uint16_t kMaximumTargetBytes = 1024;
    static const std::uint16_t kMaximumFieldNameBytes = 64;
    static const std::uint16_t kMaximumFieldValueBytes = 1024;

    enum class Decision : std::uint8_t
    {
        NeedMore,
        AcceptedNormalHttp,
        AcceptedWebSocket,
        Rejected,
    };

    enum class RejectReason : std::uint8_t
    {
        None,
        InvalidPolicy,
        Method,
        Version,
        RequestTarget,
        RequestLine,
        BareLf,
        ObsFold,
        MalformedHeader,
        HeaderTooLong,
        FieldNameTooLong,
        FieldValueTooLong,
        ContentLength,
        TransferEncoding,
        MissingOrigin,
        EmptyOrigin,
        NullOrigin,
        MalformedOrigin,
        DuplicateOrigin,
        OriginComma,
        OriginTooLong,
        OriginMismatch,
        MissingUpgrade,
        DuplicateUpgrade,
        InvalidUpgrade,
        MissingConnection,
        DuplicateConnection,
        InvalidConnection,
        WrongWebSocketTarget,
    };

    struct Policy
    {
        const char* expected_origin;
        std::size_t expected_origin_length;
    };

    struct State
    {
        const Policy* policy;
        std::uint32_t request_bytes;
        std::uint16_t target_length;
        std::uint16_t value_length;
        std::uint16_t value_index;
        std::uint8_t phase;
        std::uint8_t literal_index;
        std::uint8_t field_candidates;
        std::uint8_t field_name_length;
        std::uint8_t field_kind;
        std::uint8_t flags;
        std::uint8_t value_matches;
        std::uint8_t null_candidate;
        RejectReason reject_reason;
    };

    static_assert(sizeof(State) <= 64, "O1-RX per-session state exceeds 64 bytes");

    struct Result
    {
        Decision decision;
        RejectReason reason;
        std::size_t consumed;
        std::size_t tail_offset;
        std::size_t tail_length;
        std::uint32_t completed_normal_requests;
        std::uint32_t request_header_bytes;
    };

    bool Initialize(State& state, const Policy& policy);
    bool Reset(State& state, const Policy& policy);
    Result Consume(State& state, const std::uint8_t* bytes, std::size_t length);
} // namespace ORIGIN_ADMISSION
