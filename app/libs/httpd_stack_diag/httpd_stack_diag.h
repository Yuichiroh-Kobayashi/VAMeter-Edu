#pragma once

#include <cstddef>
#include <cstdint>

namespace HTTPD_STACK_DIAG
{
    static const std::uint32_t kConfiguredStackBytes = 4096U;
    static const std::uint32_t kUnmeasured = 0xFFFFFFFFU;
    static const std::uint32_t kBreadcrumbMagic = 0x48345344U;
    static const std::uint32_t kBreadcrumbVersion = 1U;
    static const std::uint32_t kValidMarker = 0xD2B4096DU;
    static const std::uint32_t kInvalidMarker = 0U;

    enum class Stage : std::uint32_t
    {
        NONE = 0U,
        HTTP_REQUEST_ENTER = 1U,
        WS_FRAME_RECEIVE_ENTER = 2U,
        WS_FRAME_RECEIVE_COMPLETE = 3U,
        CONTROL_PARSE_COMPLETE = 4U,
        CONTROL_VALIDATE_COMPLETE = 5U,
        WELCOME_BUILD_COMPLETE = 6U,
        WELCOME_SEND_ENTER = 7U,
        WELCOME_SEND_RETURN = 8U,
        START_RESPONSE_BUILD_COMPLETE = 9U,
        START_RESPONSE_SEND_RETURN = 10U,
        READY_PING_RESPONSE_SEND_RETURN = 11U,
        STREAMING_PING_RESPONSE_SEND_RETURN = 12U,
        STOP_RESPONSE_ACCEPTED = 13U,
        STOP_RESPONSE_COMPLETED = 14U,
        VIOLATION_RESPONSE_1 = 15U,
        VIOLATION_RESPONSE_2 = 16U,
        VIOLATION_RESPONSE_3 = 17U,
        WS_CLOSE_BEGIN = 18U,
        WS_CLOSE_COMPLETE = 19U,
        COUNT = 20U,
    };

    static const std::size_t kStageCount = static_cast<std::size_t>(Stage::COUNT) - 1U;

    struct Sample
    {
        std::uint32_t initialized;
        std::uint32_t raw_high_water;
        std::uint32_t normalized_bytes;
        std::uint32_t configured_stack_bytes;
        std::uint32_t minimum_observed_bytes;
        std::uint32_t httpd_task_identity;
        std::uint32_t generation;
        std::uint32_t stream_id;
    };

    struct State
    {
        Sample stages[kStageCount];
        std::uint32_t global_minimum_bytes;
        std::uint32_t global_minimum_stage;
        std::uint32_t httpd_task_identity;
        std::uint32_t task_identity_mismatch;
        std::uint32_t configured_stack_mismatch;
        std::uint32_t update_count;
    };

    struct Breadcrumb
    {
        std::uint32_t magic;
        std::uint32_t version;
        std::uint32_t sequence;
        std::uint32_t valid_marker;
        std::uint32_t last_stage;
        std::uint32_t configured_stack_bytes;
        std::uint32_t last_raw_high_water;
        std::uint32_t last_normalized_bytes;
        std::uint32_t minimum_observed_bytes;
        std::uint32_t httpd_task_identity;
        std::uint32_t generation;
        std::uint32_t stream_id;
        std::uint32_t reset_reason_normalized;
        std::uint32_t reset_reason_raw;
        std::uint32_t checksum;
    };

    static_assert(sizeof(Breadcrumb) == 15U * sizeof(std::uint32_t), "breadcrumb must remain fixed-width");

    const char* StageName(Stage stage);
    const char* StageName(std::uint32_t stage);
    bool IsStage(Stage stage);
    std::size_t StageIndex(Stage stage);

    std::uint32_t NormalizeHighWater(std::uint32_t raw, std::uint32_t unitBytes);
    void Reset(State& state);
    void Update(State& state,
                Stage stage,
                std::uint32_t rawHighWater,
                std::uint32_t unitBytes,
                std::uint32_t configuredStackBytes,
                std::uint32_t taskIdentity,
                std::uint32_t generation,
                std::uint32_t streamId);
    bool UsableForAcceptance(const State& state);

    Breadcrumb EmptyBreadcrumb();
    Breadcrumb MakeBreadcrumb(const State& state,
                              Stage lastStage,
                              std::uint32_t sequence,
                              std::uint32_t resetReasonNormalized,
                              std::uint32_t resetReasonRaw);
    std::uint32_t BreadcrumbChecksum(const Breadcrumb& breadcrumb);
    bool IsValid(const Breadcrumb& breadcrumb);
    void Clear(Breadcrumb& breadcrumb);
    void Commit(Breadcrumb& target, const Breadcrumb& source);
    std::uint32_t NextSequence(std::uint32_t current, bool haveCurrent);
    bool IsSequenceNewer(std::uint32_t candidate, std::uint32_t incumbent);
    unsigned SelectNewest(const Breadcrumb& slot0, const Breadcrumb& slot1, bool& haveValid);
} // namespace HTTPD_STACK_DIAG
