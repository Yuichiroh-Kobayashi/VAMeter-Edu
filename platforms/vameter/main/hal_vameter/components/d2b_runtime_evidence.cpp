#include "d2b_runtime_evidence.h"
#include "d2b_reset_breadcrumb.h"

#include "d2b_vi_pipeline.h"
#include "d2b_vi_producer.h"

#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_rom_sys.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

#include <cstddef>

namespace D2B_RUNTIME_EVIDENCE
{
    namespace
    {
        static const char* kTag = "d2b-diag";
        static const std::uint32_t kBootCounterMagic = 0xD2B0E1DU;

        RTC_NOINIT_ATTR volatile std::uint32_t _bootCounterMagic = 0;
        RTC_NOINIT_ATTR volatile std::uint32_t _bootCounter = 0;
        volatile std::uint32_t _serverGeneration = 0;
        portMUX_TYPE _pump_diag_lock = portMUX_INITIALIZER_UNLOCKED;
        RUNTIME_EVIDENCE::RateLimiter _pump_schedule_accepted_limiter;
        RUNTIME_EVIDENCE::RateLimiter _pump_schedule_coalesced_limiter;
        RUNTIME_EVIDENCE::RateLimiter _pump_queue_rejected_limiter;
        RUNTIME_EVIDENCE::RateLimiter _pump_callback_begin_limiter;
        RUNTIME_EVIDENCE::RateLimiter _pump_callback_end_limiter;
        RUNTIME_EVIDENCE::RateLimiter _pump_stale_limiter;

        RUNTIME_EVIDENCE::Resource EmptyResource(RUNTIME_EVIDENCE::Event event,
                                                  RUNTIME_EVIDENCE::Reason reason,
                                                  RUNTIME_EVIDENCE::Result result,
                                                  RUNTIME_EVIDENCE::Owner owner,
                                                  std::uint32_t generation,
                                                  std::int32_t socket,
                                                  std::uint32_t streamId)
        {
            RUNTIME_EVIDENCE::Resource resource = {};
            resource.event = event;
            resource.reason = reason;
            resource.result = result;
            resource.owner = owner;
            resource.generation = generation;
            resource.server_generation = _serverGeneration;
            resource.socket = socket;
            resource.stream_id = streamId;
            return resource;
        }

        void FillHeap(RUNTIME_EVIDENCE::Resource& resource)
        {
            const std::uint32_t capabilities = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
            resource.heap_free = static_cast<std::uint32_t>(heap_caps_get_free_size(capabilities));
            resource.heap_min = static_cast<std::uint32_t>(heap_caps_get_minimum_free_size(capabilities));
            resource.heap_largest = static_cast<std::uint32_t>(heap_caps_get_largest_free_block(capabilities));
        }

        RUNTIME_EVIDENCE::Resource PipelineResource(RUNTIME_EVIDENCE::Event event,
                                                    RUNTIME_EVIDENCE::Reason reason,
                                                    RUNTIME_EVIDENCE::Result result,
                                                    const D2B_PIPELINE::OwnerKey& owner,
                                                    const D2B_PIPELINE::Snapshot& pipeline,
                                                    const D2B_PRODUCER::Snapshot& producer,
                                                    std::uint32_t streamId)
        {
            RUNTIME_EVIDENCE::Resource resource =
                EmptyResource(event,
                              reason,
                              result,
                              RUNTIME_EVIDENCE::Owner::System,
                              owner.generation,
                              owner.socket,
                              streamId);
            FillHeap(resource);
            resource.encoder_stack_min = pipeline.encoderStackHighWaterBytes;
            resource.tx_stack_min = pipeline.txStackHighWaterBytes;
            resource.acquisition_depth = producer.queuedSampleCount;
            resource.output_depth = pipeline.queuedOutputFrames;
            resource.producer_drops = producer.producerDropCount;
            resource.output_drops = pipeline.outputQueueDropCount;
            return resource;
        }

        void Emit(const RUNTIME_EVIDENCE::Resource& resource)
        {
            char line[768];
            RUNTIME_EVIDENCE::FormatLine(resource, line, sizeof(line));
            ESP_LOGI(kTag, "%s", line);
        }

        void EmitBoot(const RUNTIME_EVIDENCE::BootResource& resource)
        {
            char line[896];
            RUNTIME_EVIDENCE::FormatBootLine(resource, line, sizeof(line));
            ESP_LOGI(kTag, "%s", line);
        }

        bool PumpAllowed(RUNTIME_EVIDENCE::RateLimiter& limiter)
        {
            const std::uint64_t nowUs = static_cast<std::uint64_t>(esp_timer_get_time());
            portENTER_CRITICAL(&_pump_diag_lock);
            const bool allowed = limiter.Allow(nowUs);
            portEXIT_CRITICAL(&_pump_diag_lock);
            return allowed;
        }

        void EmitPump(const D2B_PIPELINE::OwnerKey& owner,
                      const D2B_PIPELINE::Snapshot& pipeline,
                      const D2B_PRODUCER::Snapshot& producer,
                      RUNTIME_EVIDENCE::Reason reason,
                      RUNTIME_EVIDENCE::RateLimiter& limiter)
        {
            if (!PumpAllowed(limiter))
                return;
            Emit(PipelineResource(RUNTIME_EVIDENCE::Event::Pump,
                                  reason,
                                  reason == RUNTIME_EVIDENCE::Reason::PumpQueueRejected
                                      ? RUNTIME_EVIDENCE::Result::Rejected
                                      : RUNTIME_EVIDENCE::Result::Observed,
                                  owner,
                                  pipeline,
                                  producer,
                                  pipeline.streamId));
        }
    } // namespace

    void MarkApplicationStage(BreadcrumbStage stage,
                              std::uint32_t serverGeneration,
                              std::uint32_t websocketGeneration,
                              std::int32_t socket,
                              std::uint32_t streamId)
    {
        D2B_RESET_BREADCRUMB_ADAPTER::MarkApplicationStage(
            stage, serverGeneration, websocketGeneration, socket, streamId);
    }

    void MarkHttpdStage(BreadcrumbStage stage,
                        std::uint32_t serverGeneration,
                        std::uint32_t websocketGeneration,
                        std::int32_t socket,
                        std::uint32_t streamId)
    {
        D2B_RESET_BREADCRUMB_ADAPTER::MarkHttpdStage(
            stage, serverGeneration, websocketGeneration, socket, streamId);
    }

    void ReplayPriorBreadcrumbSnapshot() { D2B_RESET_BREADCRUMB_ADAPTER::ReplayPriorSnapshot(); }

    void SetServerGeneration(std::uint32_t generation) { _serverGeneration = generation; }

    void LogServerRequest(RUNTIME_EVIDENCE::Event event,
                          RUNTIME_EVIDENCE::Owner owner,
                          RUNTIME_EVIDENCE::Reason reason,
                          std::uint32_t generation)
    {
        RUNTIME_EVIDENCE::Resource resource = EmptyResource(event,
                                                             reason,
                                                             RUNTIME_EVIDENCE::Result::Requested,
                                                             owner,
                                                             generation,
                                                             -1,
                                                             0);
        FillHeap(resource);
        Emit(resource);
    }

    void LogServerResult(RUNTIME_EVIDENCE::Event event,
                         RUNTIME_EVIDENCE::Owner owner,
                         RUNTIME_EVIDENCE::Reason reason,
                         RUNTIME_EVIDENCE::Result result,
                         std::uint32_t generation)
    {
        RUNTIME_EVIDENCE::Resource resource = EmptyResource(event,
                                                             reason,
                                                             result,
                                                             owner,
                                                             generation,
                                                             -1,
                                                             0);
        FillHeap(resource);
        Emit(resource);
    }

    void LogWebSocketConnect(const D2B_PIPELINE::OwnerKey& owner)
    {
        RUNTIME_EVIDENCE::Resource resource = EmptyResource(RUNTIME_EVIDENCE::Event::WebSocketConnect,
                                                             RUNTIME_EVIDENCE::Reason::WebSocketAccepted,
                                                             RUNTIME_EVIDENCE::Result::Accepted,
                                                             RUNTIME_EVIDENCE::Owner::System,
                                                             owner.generation,
                                                             owner.socket,
                                                             0);
        FillHeap(resource);
        Emit(resource);
    }

    void LogWebSocketDisconnect(const D2B_PIPELINE::OwnerKey& owner, RUNTIME_EVIDENCE::Reason reason)
    {
        RUNTIME_EVIDENCE::Resource resource = EmptyResource(RUNTIME_EVIDENCE::Event::WebSocketDisconnect,
                                                             reason,
                                                             RUNTIME_EVIDENCE::Result::Completed,
                                                             RUNTIME_EVIDENCE::Owner::System,
                                                             owner.generation,
                                                             owner.socket,
                                                             0);
        FillHeap(resource);
        Emit(resource);
    }

    void LogStreamStart(const D2B_PIPELINE::OwnerKey& owner,
                        std::uint32_t streamId,
                        const D2B_PIPELINE::Snapshot& pipeline,
                        const D2B_PRODUCER::Snapshot& producer)
    {
        Emit(PipelineResource(RUNTIME_EVIDENCE::Event::StreamStart,
                              RUNTIME_EVIDENCE::Reason::StreamAccepted,
                              RUNTIME_EVIDENCE::Result::Accepted,
                              owner,
                              pipeline,
                              producer,
                              streamId));
    }

    void LogStreamStopAccepted(const D2B_PIPELINE::OwnerKey& owner,
                               std::uint32_t streamId,
                               const D2B_PIPELINE::Snapshot& pipeline,
                               const D2B_PRODUCER::Snapshot& producer)
    {
        Emit(PipelineResource(RUNTIME_EVIDENCE::Event::StreamStop,
                              RUNTIME_EVIDENCE::Reason::StreamOrderlyStopAccepted,
                              RUNTIME_EVIDENCE::Result::Accepted,
                              owner,
                              pipeline,
                              producer,
                              streamId));
    }

    void LogStreamStopCompleted(const D2B_PIPELINE::OwnerKey& owner,
                                std::uint32_t streamId,
                                const D2B_PIPELINE::Snapshot& pipeline,
                                const D2B_PRODUCER::Snapshot& producer)
    {
        Emit(PipelineResource(RUNTIME_EVIDENCE::Event::StreamStop,
                              RUNTIME_EVIDENCE::Reason::StreamOrderlyStopCompleted,
                              RUNTIME_EVIDENCE::Result::Completed,
                              owner,
                              pipeline,
                              producer,
                              streamId));
    }

    void LogStreamAbrupt(const D2B_PIPELINE::OwnerKey& owner,
                         std::uint32_t streamId,
                         RUNTIME_EVIDENCE::Reason reason,
                         const D2B_PIPELINE::Snapshot& pipeline,
                         const D2B_PRODUCER::Snapshot& producer)
    {
        Emit(PipelineResource(RUNTIME_EVIDENCE::Event::StreamAbrupt,
                              reason,
                              RUNTIME_EVIDENCE::Result::Abrupt,
                              owner,
                              pipeline,
                              producer,
                              streamId));
    }

    void LogSendFailure(const D2B_PIPELINE::OwnerKey& owner,
                        std::uint32_t streamId,
                        RUNTIME_EVIDENCE::Reason reason,
                        const D2B_PIPELINE::Snapshot& pipeline,
                        const D2B_PRODUCER::Snapshot& producer)
    {
        Emit(PipelineResource(RUNTIME_EVIDENCE::Event::SendFailure,
                              reason,
                              RUNTIME_EVIDENCE::Result::Failed,
                              owner,
                              pipeline,
                              producer,
                              streamId));
    }

    void LogQueueSnapshot(const D2B_PIPELINE::OwnerKey& owner,
                          const D2B_PIPELINE::Snapshot& pipeline,
                          const D2B_PRODUCER::Snapshot& producer,
                          RUNTIME_EVIDENCE::Reason reason)
    {
        Emit(PipelineResource(RUNTIME_EVIDENCE::Event::QueueSnapshot,
                              reason,
                              RUNTIME_EVIDENCE::Result::Observed,
                              owner,
                              pipeline,
                              producer,
                              pipeline.streamId));
    }

    void LogActiveTrend(const D2B_PIPELINE::OwnerKey& owner,
                        const D2B_PIPELINE::Snapshot& pipeline,
                        const D2B_PRODUCER::Snapshot& producer)
    {
        Emit(PipelineResource(RUNTIME_EVIDENCE::Event::HeapTrend,
                              RUNTIME_EVIDENCE::Reason::ActiveStreamTrend,
                              RUNTIME_EVIDENCE::Result::Observed,
                              owner,
                              pipeline,
                              producer,
                              pipeline.streamId));
    }

    void LogPumpScheduleAccepted(const D2B_PIPELINE::OwnerKey& owner,
                                 const D2B_PIPELINE::Snapshot& pipeline,
                                 const D2B_PRODUCER::Snapshot& producer)
    {
        EmitPump(owner,
                 pipeline,
                 producer,
                 RUNTIME_EVIDENCE::Reason::PumpScheduleAccepted,
                 _pump_schedule_accepted_limiter);
    }

    void LogPumpScheduleCoalesced(const D2B_PIPELINE::OwnerKey& owner,
                                  const D2B_PIPELINE::Snapshot& pipeline,
                                  const D2B_PRODUCER::Snapshot& producer)
    {
        EmitPump(owner,
                 pipeline,
                 producer,
                 RUNTIME_EVIDENCE::Reason::PumpScheduleCoalesced,
                 _pump_schedule_coalesced_limiter);
    }

    void LogPumpQueueRejected(const D2B_PIPELINE::OwnerKey& owner,
                              const D2B_PIPELINE::Snapshot& pipeline,
                              const D2B_PRODUCER::Snapshot& producer)
    {
        EmitPump(owner, pipeline, producer, RUNTIME_EVIDENCE::Reason::PumpQueueRejected, _pump_queue_rejected_limiter);
    }

    void LogPumpCallbackBegin(const D2B_PIPELINE::OwnerKey& owner,
                              const D2B_PIPELINE::Snapshot& pipeline,
                              const D2B_PRODUCER::Snapshot& producer)
    {
        EmitPump(owner, pipeline, producer, RUNTIME_EVIDENCE::Reason::PumpCallbackBegin, _pump_callback_begin_limiter);
    }

    void LogPumpCallbackEnd(const D2B_PIPELINE::OwnerKey& owner,
                            const D2B_PIPELINE::Snapshot& pipeline,
                            const D2B_PRODUCER::Snapshot& producer)
    {
        EmitPump(owner, pipeline, producer, RUNTIME_EVIDENCE::Reason::PumpCallbackEnd, _pump_callback_end_limiter);
    }

    void LogPumpStale(const D2B_PIPELINE::OwnerKey& owner,
                      const D2B_PIPELINE::Snapshot& pipeline,
                      const D2B_PRODUCER::Snapshot& producer)
    {
        EmitPump(owner, pipeline, producer, RUNTIME_EVIDENCE::Reason::PumpStale, _pump_stale_limiter);
    }

    void LogBoot()
    {
        if (_bootCounterMagic != kBootCounterMagic)
        {
            _bootCounterMagic = kBootCounterMagic;
            _bootCounter = 0;
        }
        if (_bootCounter == 0xFFFFFFFFU)
            _bootCounter = 1;
        else
            _bootCounter = static_cast<std::uint32_t>(_bootCounter + 1U);

        const esp_reset_reason_t resetReason = esp_reset_reason();
        const std::uint32_t rawResetReason = static_cast<std::uint32_t>(esp_rom_get_reset_reason(0));
        const std::uint32_t identity = esp_random();
        D2B_RESET_BREADCRUMB_ADAPTER::BeginBoot(static_cast<std::uint32_t>(resetReason), rawResetReason);
        D2B_RESET_BREADCRUMB_ADAPTER::Record prior = {};
        const bool havePrior = D2B_RESET_BREADCRUMB_ADAPTER::ReadPriorSnapshot(prior);
        RUNTIME_EVIDENCE::BootResource resource = {};
        resource.resource = EmptyResource(RUNTIME_EVIDENCE::Event::Boot,
                                           RUNTIME_EVIDENCE::Reason::Boot,
                                           RUNTIME_EVIDENCE::Result::Observed,
                                           RUNTIME_EVIDENCE::Owner::None,
                                           0,
                                           -1,
                                           0);
        FillHeap(resource.resource);
        resource.reset_reason_code = static_cast<std::uint32_t>(resetReason);
        resource.reset_reason_raw = rawResetReason;
        resource.boot_identity = identity;
        resource.rtc_boot_counter = _bootCounter;
        resource.prior_valid = havePrior ? 1U : 0U;
        resource.prior_stage = havePrior ? prior.stage : 0U;
        resource.prior_sequence = havePrior ? prior.sequence : 0U;
        resource.prior_server_generation = havePrior ? prior.server_generation : 0U;
        resource.prior_websocket_generation = havePrior ? prior.websocket_generation : 0U;
        resource.prior_socket = havePrior ? prior.socket : -1;
        resource.prior_stream_id = havePrior ? prior.stream_id : 0U;
        resource.prior_configured_httpd_stack_bytes =
            havePrior ? prior.configured_httpd_stack_bytes : 0U;
        resource.prior_httpd_stack_high_water_raw =
            havePrior ? prior.httpd_stack_high_water_raw : D2B_RESET_BREADCRUMB::kUnmeasuredStack;
        resource.prior_httpd_stack_high_water_bytes =
            havePrior ? prior.httpd_stack_high_water_bytes : D2B_RESET_BREADCRUMB::kUnmeasuredStack;
        resource.prior_httpd_stack_sample_valid = havePrior ? prior.httpd_stack_sample_valid : 0U;
        resource.prior_internal_heap_free = havePrior ? prior.internal_heap_free : 0U;
        resource.prior_internal_heap_min = havePrior ? prior.internal_heap_min : 0U;
        resource.prior_internal_heap_largest = havePrior ? prior.internal_heap_largest : 0U;
        resource.prior_reset_reason_code = havePrior ? prior.reset_reason_code : 0U;
        resource.prior_reset_reason_raw = havePrior ? prior.reset_reason_raw : 0U;
        resource.prior_checksum = havePrior ? prior.checksum : 0U;
        EmitBoot(resource);
        D2B_RESET_BREADCRUMB_ADAPTER::MarkBootReported();
    }
} // namespace D2B_RUNTIME_EVIDENCE
