#include "httpd_stack_diag.h"

#include <atomic>
#include <limits>

namespace HTTPD_STACK_DIAG
{
    namespace
    {
        struct StageToken
        {
            Stage stage;
            const char* name;
        };

        const StageToken kStages[] = {
            {Stage::HTTP_REQUEST_ENTER, "HTTP_REQUEST_ENTER"},
            {Stage::WS_FRAME_RECEIVE_ENTER, "WS_FRAME_RECEIVE_ENTER"},
            {Stage::WS_FRAME_RECEIVE_COMPLETE, "WS_FRAME_RECEIVE_COMPLETE"},
            {Stage::CONTROL_PARSE_COMPLETE, "CONTROL_PARSE_COMPLETE"},
            {Stage::CONTROL_VALIDATE_COMPLETE, "CONTROL_VALIDATE_COMPLETE"},
            {Stage::WELCOME_BUILD_COMPLETE, "WELCOME_BUILD_COMPLETE"},
            {Stage::WELCOME_SEND_ENTER, "WELCOME_SEND_ENTER"},
            {Stage::WELCOME_SEND_RETURN, "WELCOME_SEND_RETURN"},
            {Stage::START_RESPONSE_BUILD_COMPLETE, "START_RESPONSE_BUILD_COMPLETE"},
            {Stage::START_RESPONSE_SEND_RETURN, "START_RESPONSE_SEND_RETURN"},
            {Stage::READY_PING_RESPONSE_SEND_RETURN, "READY_PING_RESPONSE_SEND_RETURN"},
            {Stage::STREAMING_PING_RESPONSE_SEND_RETURN, "STREAMING_PING_RESPONSE_SEND_RETURN"},
            {Stage::STOP_RESPONSE_ACCEPTED, "STOP_RESPONSE_ACCEPTED"},
            {Stage::STOP_RESPONSE_COMPLETED, "STOP_RESPONSE_COMPLETED"},
            {Stage::VIOLATION_RESPONSE_1, "VIOLATION_RESPONSE_1"},
            {Stage::VIOLATION_RESPONSE_2, "VIOLATION_RESPONSE_2"},
            {Stage::VIOLATION_RESPONSE_3, "VIOLATION_RESPONSE_3"},
            {Stage::WS_CLOSE_BEGIN, "WS_CLOSE_BEGIN"},
            {Stage::WS_CLOSE_COMPLETE, "WS_CLOSE_COMPLETE"},
        };

        std::uint32_t CrcByte(std::uint32_t crc, std::uint8_t value)
        {
            crc ^= value;
            for (unsigned bit = 0U; bit < 8U; ++bit)
                crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
            return crc;
        }

        std::uint32_t CrcWord(std::uint32_t crc, std::uint32_t value)
        {
            for (unsigned shift = 0U; shift < 32U; shift += 8U)
                crc = CrcByte(crc, static_cast<std::uint8_t>((value >> shift) & 0xFFU));
            return crc;
        }
    } // namespace

    const char* StageName(Stage stage) { return StageName(static_cast<std::uint32_t>(stage)); }

    const char* StageName(std::uint32_t stage)
    {
        for (std::size_t index = 0U; index < kStageCount; ++index)
        {
            if (static_cast<std::uint32_t>(kStages[index].stage) == stage)
                return kStages[index].name;
        }
        return stage == 0U ? "NONE" : "UNKNOWN";
    }

    bool IsStage(Stage stage)
    {
        const std::uint32_t value = static_cast<std::uint32_t>(stage);
        return value > 0U && value < static_cast<std::uint32_t>(Stage::COUNT);
    }

    std::size_t StageIndex(Stage stage)
    {
        return IsStage(stage) ? static_cast<std::size_t>(static_cast<std::uint32_t>(stage) - 1U) : kStageCount;
    }

    std::uint32_t NormalizeHighWater(std::uint32_t raw, std::uint32_t unitBytes)
    {
        if (raw == 0U || unitBytes == 0U)
            return 0U;
        const std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max();
        return raw > maximum / unitBytes ? maximum : raw * unitBytes;
    }

    void Reset(State& state)
    {
        state = {};
        state.global_minimum_bytes = kUnmeasured;
        state.global_minimum_stage = static_cast<std::uint32_t>(Stage::NONE);
        for (std::size_t index = 0U; index < kStageCount; ++index)
            state.stages[index].minimum_observed_bytes = kUnmeasured;
    }

    void Update(State& state,
                Stage stage,
                std::uint32_t rawHighWater,
                std::uint32_t unitBytes,
                std::uint32_t configuredStackBytes,
                std::uint32_t taskIdentity,
                std::uint32_t generation,
                std::uint32_t streamId)
    {
        const std::size_t index = StageIndex(stage);
        if (index >= kStageCount)
            return;
        const std::uint32_t bytes = NormalizeHighWater(rawHighWater, unitBytes);
        Sample& sample = state.stages[index];
        if (sample.initialized == 0U || bytes < sample.minimum_observed_bytes)
            sample.minimum_observed_bytes = bytes;
        sample.raw_high_water = rawHighWater;
        sample.normalized_bytes = bytes;
        sample.configured_stack_bytes = configuredStackBytes;
        sample.httpd_task_identity = taskIdentity;
        sample.generation = generation;
        sample.stream_id = streamId;
        sample.initialized = 1U;

        if (state.httpd_task_identity == 0U)
            state.httpd_task_identity = taskIdentity;
        else if (taskIdentity == 0U || state.httpd_task_identity != taskIdentity)
            state.task_identity_mismatch = 1U;
        if (configuredStackBytes != kConfiguredStackBytes)
            state.configured_stack_mismatch = 1U;
        if (bytes < state.global_minimum_bytes)
        {
            state.global_minimum_bytes = bytes;
            state.global_minimum_stage = static_cast<std::uint32_t>(stage);
        }
        ++state.update_count;
    }

    bool UsableForAcceptance(const State& state)
    {
        return state.update_count != 0U && state.global_minimum_bytes != 0U && state.global_minimum_bytes != kUnmeasured &&
               state.task_identity_mismatch == 0U && state.configured_stack_mismatch == 0U;
    }

    Breadcrumb EmptyBreadcrumb()
    {
        Breadcrumb value = {};
        value.magic = kBreadcrumbMagic;
        value.version = kBreadcrumbVersion;
        value.valid_marker = kInvalidMarker;
        value.last_stage = static_cast<std::uint32_t>(Stage::NONE);
        value.configured_stack_bytes = kConfiguredStackBytes;
        value.last_raw_high_water = kUnmeasured;
        value.last_normalized_bytes = kUnmeasured;
        value.minimum_observed_bytes = kUnmeasured;
        return value;
    }

    Breadcrumb MakeBreadcrumb(const State& state,
                              Stage lastStage,
                              std::uint32_t sequence,
                              std::uint32_t resetReasonNormalized,
                              std::uint32_t resetReasonRaw)
    {
        Breadcrumb value = EmptyBreadcrumb();
        value.sequence = sequence;
        value.last_stage = static_cast<std::uint32_t>(lastStage);
        const std::size_t index = StageIndex(lastStage);
        if (index < kStageCount && state.stages[index].initialized != 0U)
        {
            const Sample& sample = state.stages[index];
            value.configured_stack_bytes = sample.configured_stack_bytes;
            value.last_raw_high_water = sample.raw_high_water;
            value.last_normalized_bytes = sample.normalized_bytes;
            value.httpd_task_identity = sample.httpd_task_identity;
            value.generation = sample.generation;
            value.stream_id = sample.stream_id;
        }
        value.minimum_observed_bytes = state.global_minimum_bytes;
        value.reset_reason_normalized = resetReasonNormalized;
        value.reset_reason_raw = resetReasonRaw;
        value.checksum = BreadcrumbChecksum(value);
        return value;
    }

    std::uint32_t BreadcrumbChecksum(const Breadcrumb& value)
    {
        std::uint32_t crc = 0xFFFFFFFFU;
        crc = CrcWord(crc, value.magic);
        crc = CrcWord(crc, value.version);
        crc = CrcWord(crc, value.sequence);
        crc = CrcWord(crc, value.last_stage);
        crc = CrcWord(crc, value.configured_stack_bytes);
        crc = CrcWord(crc, value.last_raw_high_water);
        crc = CrcWord(crc, value.last_normalized_bytes);
        crc = CrcWord(crc, value.minimum_observed_bytes);
        crc = CrcWord(crc, value.httpd_task_identity);
        crc = CrcWord(crc, value.generation);
        crc = CrcWord(crc, value.stream_id);
        crc = CrcWord(crc, value.reset_reason_normalized);
        crc = CrcWord(crc, value.reset_reason_raw);
        return crc ^ 0xFFFFFFFFU;
    }

    bool IsValid(const Breadcrumb& value)
    {
        return value.magic == kBreadcrumbMagic && value.version == kBreadcrumbVersion && value.sequence != 0U &&
               value.valid_marker == kValidMarker &&
               (value.last_stage == 0U || IsStage(static_cast<Stage>(value.last_stage))) &&
               value.checksum == BreadcrumbChecksum(value);
    }

    void Clear(Breadcrumb& breadcrumb) { breadcrumb = EmptyBreadcrumb(); }

    void Commit(Breadcrumb& target, const Breadcrumb& source)
    {
        target.valid_marker = kInvalidMarker;
        std::atomic_thread_fence(std::memory_order_release);
        target = source;
        target.checksum = BreadcrumbChecksum(target);
        std::atomic_thread_fence(std::memory_order_release);
        target.valid_marker = kValidMarker;
    }

    std::uint32_t NextSequence(std::uint32_t current, bool haveCurrent)
    {
        if (!haveCurrent || current == 0U || current == std::numeric_limits<std::uint32_t>::max())
            return 1U;
        return current + 1U;
    }

    bool IsSequenceNewer(std::uint32_t candidate, std::uint32_t incumbent)
    {
        if (candidate == 0U || incumbent == 0U || candidate == incumbent)
            return false;
        const std::uint32_t difference = candidate - incumbent;
        return difference != 0x80000000U && difference < 0x80000000U;
    }

    unsigned SelectNewest(const Breadcrumb& slot0, const Breadcrumb& slot1, bool& haveValid)
    {
        const bool valid0 = IsValid(slot0);
        const bool valid1 = IsValid(slot1);
        haveValid = valid0 || valid1;
        if (!valid0)
            return valid1 ? 1U : 0U;
        if (!valid1)
            return 0U;
        return IsSequenceNewer(slot1.sequence, slot0.sequence) ? 1U : 0U;
    }
} // namespace HTTPD_STACK_DIAG
