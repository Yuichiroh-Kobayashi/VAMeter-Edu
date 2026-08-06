#include "d2b_httpd_stack_diag.h"

#include <esp_attr.h>
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>

namespace D2B_HTTPD_STACK_DIAG
{
    namespace
    {
        static const char* kTag = "d2b-httpd-stack";

        HTTPD_STACK_DIAG::State _state;
        HTTPD_STACK_DIAG::Breadcrumb _prior;
        HTTPD_STACK_DIAG::Breadcrumb _current;
        HTTPD_STACK_DIAG::Breadcrumb _scratch;
        RTC_NOINIT_ATTR volatile HTTPD_STACK_DIAG::Breadcrumb _slots[2] = {};

        std::uint32_t _resetReasonNormalized = 0U;
        std::uint32_t _resetReasonRaw = 0U;
        unsigned _currentSlot = 0U;
        bool _haveCurrent = false;
        bool _havePrior = false;

        void ReadVolatile(const volatile HTTPD_STACK_DIAG::Breadcrumb& source, HTTPD_STACK_DIAG::Breadcrumb& destination)
        {
            destination.magic = source.magic;
            destination.version = source.version;
            destination.sequence = source.sequence;
            destination.valid_marker = source.valid_marker;
            destination.last_stage = source.last_stage;
            destination.configured_stack_bytes = source.configured_stack_bytes;
            destination.last_raw_high_water = source.last_raw_high_water;
            destination.last_normalized_bytes = source.last_normalized_bytes;
            destination.minimum_observed_bytes = source.minimum_observed_bytes;
            destination.httpd_task_identity = source.httpd_task_identity;
            destination.generation = source.generation;
            destination.stream_id = source.stream_id;
            destination.reset_reason_normalized = source.reset_reason_normalized;
            destination.reset_reason_raw = source.reset_reason_raw;
            destination.checksum = source.checksum;
        }

        bool ReadSlot(unsigned index, HTTPD_STACK_DIAG::Breadcrumb& destination)
        {
            if (index >= 2U || _slots[index].valid_marker != HTTPD_STACK_DIAG::kValidMarker)
                return false;
            std::atomic_thread_fence(std::memory_order_acquire);
            ReadVolatile(_slots[index], destination);
            return HTTPD_STACK_DIAG::IsValid(destination);
        }

        void StoreVolatile(volatile HTTPD_STACK_DIAG::Breadcrumb& target, const HTTPD_STACK_DIAG::Breadcrumb& source)
        {
            target.valid_marker = HTTPD_STACK_DIAG::kInvalidMarker;
            std::atomic_thread_fence(std::memory_order_release);
            target.magic = source.magic;
            target.version = source.version;
            target.sequence = source.sequence;
            target.last_stage = source.last_stage;
            target.configured_stack_bytes = source.configured_stack_bytes;
            target.last_raw_high_water = source.last_raw_high_water;
            target.last_normalized_bytes = source.last_normalized_bytes;
            target.minimum_observed_bytes = source.minimum_observed_bytes;
            target.httpd_task_identity = source.httpd_task_identity;
            target.generation = source.generation;
            target.stream_id = source.stream_id;
            target.reset_reason_normalized = source.reset_reason_normalized;
            target.reset_reason_raw = source.reset_reason_raw;
            target.checksum = HTTPD_STACK_DIAG::BreadcrumbChecksum(source);
            std::atomic_thread_fence(std::memory_order_release);
            target.valid_marker = HTTPD_STACK_DIAG::kValidMarker;
        }

        void Persist(Stage stage)
        {
            const std::uint32_t sequence = HTTPD_STACK_DIAG::NextSequence(_haveCurrent ? _current.sequence : 0U, _haveCurrent);
            const unsigned targetSlot = _haveCurrent ? (_currentSlot == 0U ? 1U : 0U) : 0U;
            _scratch = HTTPD_STACK_DIAG::MakeBreadcrumb(_state, stage, sequence, _resetReasonNormalized, _resetReasonRaw);
            StoreVolatile(_slots[targetSlot], _scratch);
            _current = _scratch;
            _current.valid_marker = HTTPD_STACK_DIAG::kValidMarker;
            _currentSlot = targetSlot;
            _haveCurrent = true;
        }
    } // namespace

    void BootInitialize(std::uint32_t resetReasonNormalized, std::uint32_t resetReasonRaw)
    {
        HTTPD_STACK_DIAG::Breadcrumb slot0 = HTTPD_STACK_DIAG::EmptyBreadcrumb();
        HTTPD_STACK_DIAG::Breadcrumb slot1 = HTTPD_STACK_DIAG::EmptyBreadcrumb();
        const bool valid0 = ReadSlot(0U, slot0);
        const bool valid1 = ReadSlot(1U, slot1);
        bool haveValid = false;
        const unsigned newest = HTTPD_STACK_DIAG::SelectNewest(slot0, slot1, haveValid);
        _havePrior = haveValid && (valid0 || valid1);
        _prior = _havePrior ? (newest == 0U ? slot0 : slot1) : HTTPD_STACK_DIAG::EmptyBreadcrumb();
        _current = _prior;
        _currentSlot = newest;
        _haveCurrent = _havePrior;
        _resetReasonNormalized = resetReasonNormalized;
        _resetReasonRaw = resetReasonRaw;
        HTTPD_STACK_DIAG::Reset(_state);
    }

    void LogBootPrior()
    {
        ESP_LOGI(kTag,
                 "HTTPD_STACK_DIAG_PRIOR current_reset_reason=%lu current_reset_reason_raw=%lu prior_valid=%u "
                 "prior_disposition=%s prior_stage=%s prior_raw_high_water=%lu prior_normalized_bytes=%lu "
                 "prior_minimum_bytes=%lu prior_configured=%lu prior_generation=%lu prior_stream_id=%lu",
                 static_cast<unsigned long>(_resetReasonNormalized),
                 static_cast<unsigned long>(_resetReasonRaw),
                 _havePrior ? 1U : 0U,
                 _havePrior ? "STALE_FROM_PRIOR_BOOT" : "NO_VALID_PRIOR",
                 HTTPD_STACK_DIAG::StageName(_havePrior ? _prior.last_stage : 0U),
                 static_cast<unsigned long>(_havePrior ? _prior.last_raw_high_water : HTTPD_STACK_DIAG::kUnmeasured),
                 static_cast<unsigned long>(_havePrior ? _prior.last_normalized_bytes : HTTPD_STACK_DIAG::kUnmeasured),
                 static_cast<unsigned long>(_havePrior ? _prior.minimum_observed_bytes : HTTPD_STACK_DIAG::kUnmeasured),
                 static_cast<unsigned long>(_havePrior ? _prior.configured_stack_bytes : 0U),
                 static_cast<unsigned long>(_havePrior ? _prior.generation : 0U),
                 static_cast<unsigned long>(_havePrior ? _prior.stream_id : 0U));
    }

    void LogConfiguredStack()
    {
        ESP_LOGI(kTag,
                 "HTTPD_STACK_DIAG_START configured_httpd_stack_bytes=%lu state_bytes=%lu rtc_bytes=%lu",
                 static_cast<unsigned long>(HTTPD_STACK_DIAG::kConfiguredStackBytes),
                 static_cast<unsigned long>(StaticStateBytes()),
                 static_cast<unsigned long>(RtcStateBytes()));
    }

    void Capture(Stage stage, std::uint32_t generation, std::uint32_t streamId)
    {
        static_assert(sizeof(StackType_t) == 1U, "ESP32-S3 StackType_t unit must be one byte");
        const UBaseType_t raw = uxTaskGetStackHighWaterMark(nullptr);
        const TaskHandle_t task = xTaskGetCurrentTaskHandle();
        const std::uint32_t taskIdentity = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(task));
        HTTPD_STACK_DIAG::Update(_state,
                                 stage,
                                 static_cast<std::uint32_t>(raw),
                                 static_cast<std::uint32_t>(sizeof(StackType_t)),
                                 HTTPD_STACK_DIAG::kConfiguredStackBytes,
                                 taskIdentity,
                                 generation,
                                 streamId);
        Persist(stage);
    }

    void EmitSnapshot()
    {
        for (std::size_t index = 0U; index < HTTPD_STACK_DIAG::kStageCount; ++index)
        {
            const HTTPD_STACK_DIAG::Sample& sample = _state.stages[index];
            if (sample.initialized == 0U)
                continue;
            const Stage stage = static_cast<Stage>(index + 1U);
            ESP_LOGI(kTag,
                     "HTTPD_STACK_DIAG stage=%s raw_high_water=%lu normalized_bytes=%lu "
                     "configured_stack_bytes=%lu minimum_observed_bytes=%lu httpd_task=0x%08lx "
                     "generation=%lu stream_id=%lu",
                     HTTPD_STACK_DIAG::StageName(stage),
                     static_cast<unsigned long>(sample.raw_high_water),
                     static_cast<unsigned long>(sample.normalized_bytes),
                     static_cast<unsigned long>(sample.configured_stack_bytes),
                     static_cast<unsigned long>(sample.minimum_observed_bytes),
                     static_cast<unsigned long>(sample.httpd_task_identity),
                     static_cast<unsigned long>(sample.generation),
                     static_cast<unsigned long>(sample.stream_id));
        }
        ESP_LOGI(kTag,
                 "HTTPD_STACK_DIAG_SUMMARY configured=%lu minimum_bytes=%lu minimum_stage=%s "
                 "updates=%lu task_identity_mismatch=%lu configured_mismatch=%lu acceptance_usable=%u",
                 static_cast<unsigned long>(HTTPD_STACK_DIAG::kConfiguredStackBytes),
                 static_cast<unsigned long>(_state.global_minimum_bytes),
                 HTTPD_STACK_DIAG::StageName(_state.global_minimum_stage),
                 static_cast<unsigned long>(_state.update_count),
                 static_cast<unsigned long>(_state.task_identity_mismatch),
                 static_cast<unsigned long>(_state.configured_stack_mismatch),
                 HTTPD_STACK_DIAG::UsableForAcceptance(_state) ? 1U : 0U);
    }

    std::size_t StaticStateBytes()
    {
        return sizeof(_state) + sizeof(_prior) + sizeof(_current) + sizeof(_scratch) +
               sizeof(_resetReasonNormalized) + sizeof(_resetReasonRaw) + sizeof(_currentSlot) + sizeof(_haveCurrent) +
               sizeof(_havePrior);
    }

    std::size_t RtcStateBytes() { return sizeof(_slots); }
} // namespace D2B_HTTPD_STACK_DIAG
