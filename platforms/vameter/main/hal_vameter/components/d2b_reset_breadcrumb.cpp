#include "d2b_reset_breadcrumb.h"

#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstddef>
#include <limits>

#ifndef VAMETER_D2B_HTTPD_STACK_SIZE
#define VAMETER_D2B_HTTPD_STACK_SIZE 4096
#endif

static_assert(VAMETER_D2B_HTTPD_STACK_SIZE >= 4096, "D2B HTTPD stack must be at least 4096 bytes");
static_assert(VAMETER_D2B_HTTPD_STACK_SIZE <= 16384, "D2B HTTPD stack exceeds supported bound");

namespace D2B_RESET_BREADCRUMB_ADAPTER
{
    namespace
    {
        static const char* kTag = "d2b-breadcrumb";

        // RTC_NOINIT_ATTR is intentionally applied to both slots, not to a
        // compiler-packed wrapper.  Every field is copied explicitly below.
        RTC_NOINIT_ATTR volatile Record _slots[2] = {};

        Record _prior = {};
        std::uint32_t _resetReasonCode = 0U;
        std::uint32_t _resetReasonRaw = 0U;
        unsigned _currentSlot = 0U;
        bool _haveCurrent = false;
        bool _havePrior = false;
        bool _bootBegan = false;
        bool _initialized = false;

        // Normal-RAM scratch for the phase/single-writer contract.  Breadcrumb
        // updates are not re-entrant and are never called concurrently; these
        // objects intentionally are not RTC storage and keep HTTPD frames
        // small while preserving field-by-field torn-write safety.
        Record _currentScratch = {};
        Record _slot0Scratch = {};
        Record _slot1Scratch = {};
        Record _nextScratch = {};

        std::uint32_t BoundedU32(std::size_t value)
        {
            return value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(value);
        }

        void CaptureHeap(std::uint32_t& freeBytes, std::uint32_t& minimumBytes, std::uint32_t& largestBytes)
        {
            const std::uint32_t capabilities = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
            freeBytes = BoundedU32(heap_caps_get_free_size(capabilities));
            minimumBytes = BoundedU32(heap_caps_get_minimum_free_size(capabilities));
            largestBytes = BoundedU32(heap_caps_get_largest_free_block(capabilities));
        }

        void ReadVolatileRecord(const volatile Record& source, Record& destination)
        {
            destination.magic = source.magic;
            destination.format_version = source.format_version;
            destination.sequence = source.sequence;
            destination.valid_marker = source.valid_marker;
            destination.stage = source.stage;
            destination.stage_inverse = source.stage_inverse;
            destination.server_generation = source.server_generation;
            destination.websocket_generation = source.websocket_generation;
            destination.socket = source.socket;
            destination.stream_id = source.stream_id;
            destination.configured_httpd_stack_bytes = source.configured_httpd_stack_bytes;
            destination.httpd_stack_high_water_raw = source.httpd_stack_high_water_raw;
            destination.httpd_stack_high_water_bytes = source.httpd_stack_high_water_bytes;
            destination.internal_heap_free = source.internal_heap_free;
            destination.internal_heap_min = source.internal_heap_min;
            destination.internal_heap_largest = source.internal_heap_largest;
            destination.reset_reason_code = source.reset_reason_code;
            destination.reset_reason_raw = source.reset_reason_raw;
            destination.httpd_stack_sample_valid = source.httpd_stack_sample_valid;
            destination.checksum = source.checksum;
        }

        bool ReadSlot(unsigned index, Record& record)
        {
            if (index >= 2U)
                return false;
            // The marker is aligned and read first; a writer invalidates it
            // before touching any body field and stores it last with release
            // ordering.
            if (_slots[index].valid_marker != D2B_RESET_BREADCRUMB::kValidMarker)
                return false;
            std::atomic_thread_fence(std::memory_order_acquire);
            ReadVolatileRecord(_slots[index], record);
            return D2B_RESET_BREADCRUMB::IsValid(record);
        }

        bool ReadBoth(Record& slot0, Record& slot1)
        {
            const bool valid0 = ReadSlot(0U, slot0);
            const bool valid1 = ReadSlot(1U, slot1);
            return valid0 || valid1;
        }

        void InitializeState()
        {
            if (_initialized)
                return;
            _slot0Scratch = {};
            _slot1Scratch = {};
            const bool any = ReadBoth(_slot0Scratch, _slot1Scratch);
            bool haveValid = false;
            const unsigned newest =
                D2B_RESET_BREADCRUMB::SelectNewestSlot(_slot0Scratch, _slot1Scratch, haveValid);
            _haveCurrent = any && haveValid;
            _havePrior = D2B_RESET_BREADCRUMB::CapturePriorSnapshot(_slot0Scratch, _slot1Scratch, _prior);
            _currentSlot = newest;
            _initialized = true;
        }

        void StoreVolatileRecord(volatile Record& target, const Record& source)
        {
            target.valid_marker = D2B_RESET_BREADCRUMB::kInvalidMarker;
            std::atomic_thread_fence(std::memory_order_release);
            target.magic = source.magic;
            target.format_version = source.format_version;
            target.sequence = source.sequence;
            target.stage = source.stage;
            target.stage_inverse = source.stage_inverse;
            target.server_generation = source.server_generation;
            target.websocket_generation = source.websocket_generation;
            target.socket = source.socket;
            target.stream_id = source.stream_id;
            target.configured_httpd_stack_bytes = source.configured_httpd_stack_bytes;
            target.httpd_stack_high_water_raw = source.httpd_stack_high_water_raw;
            target.httpd_stack_high_water_bytes = source.httpd_stack_high_water_bytes;
            target.internal_heap_free = source.internal_heap_free;
            target.internal_heap_min = source.internal_heap_min;
            target.internal_heap_largest = source.internal_heap_largest;
            target.reset_reason_code = source.reset_reason_code;
            target.reset_reason_raw = source.reset_reason_raw;
            target.httpd_stack_sample_valid = source.httpd_stack_sample_valid;
            target.checksum = D2B_RESET_BREADCRUMB::CanonicalChecksum(source);
            std::atomic_thread_fence(std::memory_order_release);
            // valid_marker is a naturally aligned uint32_t final store.
            target.valid_marker = D2B_RESET_BREADCRUMB::kValidMarker;
        }

        void CommitStage(Stage stage,
                         std::uint32_t serverGeneration,
                         std::uint32_t websocketGeneration,
                         std::int32_t socket,
                         std::uint32_t streamId,
                         std::uint32_t stackRaw,
                         std::uint32_t stackBytes,
                         std::uint32_t stackSampleValid)
        {
            InitializeState();
            _currentScratch = {};
            _slot0Scratch = {};
            _slot1Scratch = {};
            _nextScratch = {};
            bool haveCurrent = false;
            if (_haveCurrent)
                haveCurrent = ReadSlot(_currentSlot, _currentScratch);
            if (!haveCurrent)
            {
                const bool any = ReadBoth(_slot0Scratch, _slot1Scratch);
                bool anyValid = false;
                _currentSlot = D2B_RESET_BREADCRUMB::SelectNewestSlot(
                    _slot0Scratch, _slot1Scratch, anyValid);
                _haveCurrent = any && anyValid;
                if (_haveCurrent)
                    _currentScratch = _currentSlot == 0U ? _slot0Scratch : _slot1Scratch;
            }

            const std::uint32_t sequence = D2B_RESET_BREADCRUMB::NextSequence(
                _haveCurrent ? _currentScratch.sequence : 0U, _haveCurrent);
            const unsigned targetSlot = _haveCurrent ? (_currentSlot == 0U ? 1U : 0U) : 0U;
            _nextScratch = D2B_RESET_BREADCRUMB::MakeRecord(
                stage,
                sequence,
                serverGeneration,
                websocketGeneration,
                socket,
                streamId,
                static_cast<std::uint32_t>(VAMETER_D2B_HTTPD_STACK_SIZE),
                stackRaw,
                stackBytes,
                stackSampleValid,
                0U,
                0U,
                0U,
                _resetReasonCode,
                _resetReasonRaw);
            CaptureHeap(_nextScratch.internal_heap_free,
                        _nextScratch.internal_heap_min,
                        _nextScratch.internal_heap_largest);
            _nextScratch.checksum = D2B_RESET_BREADCRUMB::CanonicalChecksum(_nextScratch);
            StoreVolatileRecord(_slots[targetSlot], _nextScratch);
            _currentSlot = targetSlot;
            _haveCurrent = true;
        }
    } // namespace

    void BeginBoot(std::uint32_t resetReasonCode, std::uint32_t resetReasonRaw)
    {
        InitializeState();
        _resetReasonCode = resetReasonCode;
        _resetReasonRaw = resetReasonRaw;
        _bootBegan = true;
    }

    void MarkBootReported()
    {
        if (!_bootBegan)
            BeginBoot(0U, 0U);
        CommitStage(Stage::BOOT_REPORTED, 0U, 0U, -1, 0U, D2B_RESET_BREADCRUMB::kUnmeasuredStack,
                    D2B_RESET_BREADCRUMB::kUnmeasuredStack, 0U);
        _bootBegan = false;
    }

    void LogBoot(std::uint32_t resetReasonCode, std::uint32_t resetReasonRaw)
    {
        BeginBoot(resetReasonCode, resetReasonRaw);
        MarkBootReported();
    }

    void ReplayPriorSnapshot()
    {
        InitializeState();
        char line[768];
        const std::size_t required = D2B_RESET_BREADCRUMB::FormatCapturedPriorSnapshot(
            _prior, _havePrior, line, sizeof(line));
        (void)required;
        ESP_LOGI(kTag,
                 "prior_snapshot_replay %s current_reset_reason_code=%lu current_reset_reason_raw=%lu",
                 line,
                 static_cast<unsigned long>(_resetReasonCode),
                 static_cast<unsigned long>(_resetReasonRaw));
    }

    void MarkApplicationStage(Stage stage,
                               std::uint32_t serverGeneration,
                               std::uint32_t websocketGeneration,
                               std::int32_t socket,
                               std::uint32_t streamId)
    {
        CommitStage(stage,
                    serverGeneration,
                    websocketGeneration,
                    socket,
                    streamId,
                    D2B_RESET_BREADCRUMB::kUnmeasuredStack,
                    D2B_RESET_BREADCRUMB::kUnmeasuredStack,
                    0U);
    }

    void MarkHttpdStage(Stage stage,
                        std::uint32_t serverGeneration,
                        std::uint32_t websocketGeneration,
                        std::int32_t socket,
                        std::uint32_t streamId)
    {
        const UBaseType_t raw = uxTaskGetStackHighWaterMark(nullptr);
        const std::uint32_t rawValue = static_cast<std::uint32_t>(raw);
        const std::size_t unit = sizeof(StackType_t);
        const std::uint32_t bytes = rawValue > std::numeric_limits<std::uint32_t>::max() / unit
                                        ? std::numeric_limits<std::uint32_t>::max()
                                        : rawValue * static_cast<std::uint32_t>(unit);
        CommitStage(stage, serverGeneration, websocketGeneration, socket, streamId, rawValue, bytes, 1U);
    }

    bool ReadLatest(Record& record)
    {
        InitializeState();
        _slot0Scratch = {};
        _slot1Scratch = {};
        const bool valid0 = ReadSlot(0U, _slot0Scratch);
        const bool valid1 = ReadSlot(1U, _slot1Scratch);
        if (!valid0 && !valid1)
            return false;
        bool ignored = false;
        const unsigned selected =
            D2B_RESET_BREADCRUMB::SelectNewestSlot(_slot0Scratch, _slot1Scratch, ignored);
        record = selected == 0U ? _slot0Scratch : _slot1Scratch;
        return true;
    }

    bool ReadPriorSnapshot(Record& record)
    {
        InitializeState();
        if (!_havePrior)
            return false;
        record = _prior;
        return true;
    }

    const char* StageName(std::uint32_t stage) { return D2B_RESET_BREADCRUMB::StageName(stage); }
} // namespace D2B_RESET_BREADCRUMB_ADAPTER
