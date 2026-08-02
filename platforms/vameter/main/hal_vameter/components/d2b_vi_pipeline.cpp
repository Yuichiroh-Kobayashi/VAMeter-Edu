#include "d2b_vi_pipeline.h"

#include "d2b_vi_producer.h"
#include "d2b_runtime_evidence.h"
#include "libs/d2b_vi/d2b_frame_writer.h"
#include "libs/d2b_vi/d2b_httpd_lifecycle_state.h"
#include "libs/d2b_vi/d2b_httpd_send_pump_state.h"
#include "libs/d2b_vi/d2b_output_ring.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cstring>

namespace D2B_PIPELINE
{
    namespace
    {
        static const char* kTag = "d2b-pipeline";
        static const std::uint32_t kEncoderStackBytes = 3072;
        static const std::uint32_t kTxStackBytes = 4096;
        static const std::size_t kMaximumStoppedResponseSize = 192;
        static const unsigned kPumpFramesPerCallback = 4;

        portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
        D2B::OutputRing _output;
        OwnerKey _owner = {};
        bool _initialized = false;
        bool _owner_open = false;
        bool _stream_active = false;
        bool _stopping = false;
        bool _encoder_busy = false;
        bool _task_fault = false;
        bool _first_transport_frame = true;
        std::uint32_t _stream_id = 0;
        std::uint64_t _expected_sequence = 0;
        std::uint64_t _end_timestamp_us = 0;
        char _stopped_response[kMaximumStoppedResponseSize] = {};
        std::size_t _stopped_response_size = 0;
        std::uint32_t _encoder_stack_high_water = kEncoderStackBytes;
        std::uint32_t _tx_stack_high_water = kTxStackBytes;

        bool _pending_failure = false;
        bool _pending_failure_task_fault = false;
        RUNTIME_EVIDENCE::Reason _pending_failure_reason = RUNTIME_EVIDENCE::Reason::SendFailure;
        OwnerKey _pending_failure_owner = {};
        std::uint32_t _pending_failure_stream_id = 0;
        std::uint64_t _last_diag_producer_drops = 0;
        std::uint64_t _last_diag_output_drops = 0;
        RUNTIME_EVIDENCE::RateLimiter _diag_trend_limiter;
        D2B_HTTPD_SEND_PUMP::State _pump;
        D2B_HTTPD_LIFECYCLE::State _lifecycle;

        StaticSemaphore_t _send_mutex_storage;
        SemaphoreHandle_t _send_mutex = nullptr;
        StaticTask_t _encoder_task_storage;
        StaticTask_t _tx_task_storage;
        StackType_t _encoder_stack[kEncoderStackBytes];
        StackType_t _tx_stack[kTxStackBytes];
        TaskHandle_t _encoder_task = nullptr;
        TaskHandle_t _tx_task = nullptr;

        bool SameOwner(const OwnerKey& left, const OwnerKey& right)
        {
            return left.server == right.server && left.socket == right.socket && left.generation == right.generation &&
                   left.server != nullptr && left.socket >= 0 && left.generation != 0;
        }

        bool OwnerMatchesLocked(const OwnerKey& owner) { return _owner_open && SameOwner(_owner, owner); }

        std::uintptr_t HandleKey(httpd_handle_t server)
        {
            return reinterpret_cast<std::uintptr_t>(server);
        }

        bool LifecycleAcceptingLocked(httpd_handle_t server)
        {
            const D2B_HTTPD_LIFECYCLE::Snapshot lifecycle = _lifecycle.snapshot();
            return lifecycle.accepting && lifecycle.handleKey == HandleKey(server);
        }

        void ClearPipelineLocked(std::uint32_t& streamId, OwnerKey& abruptOwner)
        {
            (void)_pump.invalidate(_owner.generation);
            streamId = _stream_id;
            if (streamId != 0 && _stream_active)
                abruptOwner = _owner;
            _owner = {};
            _owner_open = false;
            _stream_active = false;
            _stopping = false;
            _stream_id = 0;
            _output.clear();
            _stopped_response_size = 0;
        }

        void FailStream(const OwnerKey& owner, std::uint32_t streamId, bool taskFault, bool fromPump);
        void PumpWork(void* argument);
        void RequestPump(const OwnerKey& owner);

        void UpdateHighWater(std::uint32_t& stored)
        {
            const std::uint32_t current = static_cast<std::uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
            portENTER_CRITICAL(&_lock);
            if (current < stored)
                stored = current;
            portEXIT_CRITICAL(&_lock);
        }

        void WakeTasks()
        {
            if (_encoder_task != nullptr)
                xTaskNotifyGive(_encoder_task);
            if (_tx_task != nullptr)
                xTaskNotifyGive(_tx_task);
        }

        void FailStream(const OwnerKey& owner, std::uint32_t streamId, bool taskFault, bool fromPump)
        {
            bool sendMutexHeld = fromPump;
            if (!sendMutexHeld && _send_mutex != nullptr)
                sendMutexHeld = xSemaphoreTake(_send_mutex, portMAX_DELAY) == pdTRUE;
            bool matched = false;
            bool triggerClose = false;
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId)
            {
                const D2B_HTTPD_LIFECYCLE::Snapshot lifecycle = _lifecycle.snapshot();
                triggerClose = lifecycle.accepting && lifecycle.handleKey == HandleKey(owner.server);
                (void)_pump.invalidate(owner.generation);
                _task_fault = _task_fault || taskFault;
                _pending_failure = true;
                _pending_failure_task_fault = taskFault;
                _pending_failure_owner = owner;
                _pending_failure_stream_id = streamId;
                _pending_failure_reason = taskFault ? RUNTIME_EVIDENCE::Reason::InternalFailure
                                                    : RUNTIME_EVIDENCE::Reason::SendFailure;
                matched = true;
            }
            portEXIT_CRITICAL(&_lock);
            if (!matched)
            {
                if (sendMutexHeld && !fromPump)
                    xSemaphoreGive(_send_mutex);
                return;
            }

            // Abort producer before clearing the pipeline while the common
            // send mutex remains held.
            D2B_PRODUCER::Abort(streamId);
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId)
            {
                OwnerKey ignoredOwner = {};
                std::uint32_t ignoredStreamId = 0;
                ClearPipelineLocked(ignoredStreamId, ignoredOwner);
            }
            portEXIT_CRITICAL(&_lock);
            if (taskFault)
                ESP_LOGE(kTag, "internal stream pipeline failure; closing owner generation=%lu",
                         static_cast<unsigned long>(owner.generation));
            else
                ESP_LOGW(kTag, "WebSocket send failed; closing owner generation=%lu",
                         static_cast<unsigned long>(owner.generation));
            WakeTasks();
            if (triggerClose)
            {
                const esp_err_t closeResult = httpd_sess_trigger_close(owner.server, owner.socket);
                if (closeResult != ESP_OK)
                    ESP_LOGE(kTag, "failed to schedule owner close: %s", esp_err_to_name(closeResult));
            }
            if (sendMutexHeld && !fromPump)
                xSemaphoreGive(_send_mutex);
        }

        void EmitPendingEvidence()
        {
            OwnerKey owner = {};
            std::uint32_t streamId = 0;
            RUNTIME_EVIDENCE::Reason reason = RUNTIME_EVIDENCE::Reason::Disconnect;
            bool failure = false;
            bool taskFault = false;
            portENTER_CRITICAL(&_lock);
            if (_pending_failure)
            {
                owner = _pending_failure_owner;
                streamId = _pending_failure_stream_id;
                reason = _pending_failure_reason;
                failure = true;
                taskFault = _pending_failure_task_fault;
                _pending_failure = false;
            }
            portEXIT_CRITICAL(&_lock);
            if (streamId == 0 || owner.generation == 0)
                return;

            const Snapshot pipeline = GetSnapshot();
            const D2B_PRODUCER::Snapshot producer = D2B_PRODUCER::GetSnapshot();
            if (failure && !taskFault)
            {
                D2B_RUNTIME_EVIDENCE::LogSendFailure(owner, streamId, reason, pipeline, producer);
                D2B_RUNTIME_EVIDENCE::LogStreamAbrupt(owner,
                                                      streamId,
                                                      RUNTIME_EVIDENCE::Reason::SendFailure,
                                                      pipeline,
                                                      producer);
            }
            else
            {
                D2B_RUNTIME_EVIDENCE::LogStreamAbrupt(owner, streamId, reason, pipeline, producer);
            }
        }

        void EmitTxEvidence()
        {
            OwnerKey owner = {};
            portENTER_CRITICAL(&_lock);
            const bool active = _owner_open && _stream_active;
            if (active)
                owner = _owner;
            portEXIT_CRITICAL(&_lock);
            if (!active)
                return;

            const Snapshot pipeline = GetSnapshot();
            const D2B_PRODUCER::Snapshot producer = D2B_PRODUCER::GetSnapshot();
            bool outputDropsChanged = false;
            bool producerDropsChanged = false;
            bool trendAllowed = false;
            portENTER_CRITICAL(&_lock);
            outputDropsChanged = pipeline.outputQueueDropCount != _last_diag_output_drops;
            producerDropsChanged = producer.producerDropCount != _last_diag_producer_drops;
            _last_diag_output_drops = pipeline.outputQueueDropCount;
            _last_diag_producer_drops = producer.producerDropCount;
            trendAllowed = _diag_trend_limiter.Allow(static_cast<std::uint64_t>(esp_timer_get_time()));
            portEXIT_CRITICAL(&_lock);
            if (outputDropsChanged || producerDropsChanged)
            {
                D2B_RUNTIME_EVIDENCE::LogQueueSnapshot(owner,
                                                       pipeline,
                                                       producer,
                                                       outputDropsChanged ? RUNTIME_EVIDENCE::Reason::OutputDrop
                                                                          : RUNTIME_EVIDENCE::Reason::ProducerDrop);
            }
            if (trendAllowed)
                D2B_RUNTIME_EVIDENCE::LogActiveTrend(owner, pipeline, producer);
        }

        void EncoderTask(void*)
        {
            while (true)
            {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                UpdateHighWater(_encoder_stack_high_water);
                for (std::size_t count = 0; count < D2B::kAcquisitionQueueDepth; ++count)
                {
                    portENTER_CRITICAL(&_lock);
                    _encoder_busy = true;
                    portEXIT_CRITICAL(&_lock);

                    D2B::AcquisitionSample sample = {};
                    bool producerOverflow = false;
                    std::uint32_t streamId = 0;
                    const bool available = D2B_PRODUCER::Pop(sample, producerOverflow, streamId);
                    if (!available)
                    {
                        portENTER_CRITICAL(&_lock);
                        _encoder_busy = false;
                        portEXIT_CRITICAL(&_lock);
                        if (_tx_task != nullptr)
                            xTaskNotifyGive(_tx_task);
                        break;
                    }

                    D2B::OutputFrame frame = {};
                    const D2B::ViSample vi = {
                        sample.sequence, sample.timestampUs, sample.validMask, sample.voltageV, sample.currentA};
                    const std::uint8_t flags =
                        producerOverflow ? D2B::Discontinuity | D2B::ProducerOverflow : 0;
                    const D2B::FrameWriteResult write =
                        D2B::WriteSingleViFrame(frame.data, sizeof(frame.data), streamId, flags, vi);
                    frame.size = write.size;
                    frame.sequence = sample.sequence;
                    frame.timestampUs = sample.timestampUs;

                    OwnerKey owner = {};
                    bool accepted = false;
                    portENTER_CRITICAL(&_lock);
                    if (write.ok() && _owner_open && _stream_active && _stream_id == streamId)
                    {
                        (void)_output.pushDropOldest(frame);
                        owner = _owner;
                        accepted = true;
                    }
                    _encoder_busy = false;
                    portEXIT_CRITICAL(&_lock);

                    if (!write.ok())
                    {
                        portENTER_CRITICAL(&_lock);
                        owner = _owner;
                        portEXIT_CRITICAL(&_lock);
                        FailStream(owner, streamId, true, false);
                        break;
                    }
                    if (accepted && _tx_task != nullptr)
                        xTaskNotifyGive(_tx_task);
                }
                UpdateHighWater(_encoder_stack_high_water);
            }
        }

        bool ReadyForStreamEndLocked(const D2B_PRODUCER::Snapshot& producer)
        {
            return _stopping && _stream_active && !_encoder_busy && producer.streamId == _stream_id &&
                   producer.queuedSampleCount == 0 && _output.queuedCount() == 0;
        }

        bool ReadyForStreamEnd()
        {
            const D2B_PRODUCER::Snapshot producer = D2B_PRODUCER::GetSnapshot();
            portENTER_CRITICAL(&_lock);
            const bool ready = ReadyForStreamEndLocked(producer);
            portEXIT_CRITICAL(&_lock);
            return ready;
        }

        bool CompleteOrderlyStop(const OwnerKey& owner, std::uint32_t streamId, std::uintptr_t token)
        {
            bool matched = false;
            D2B_HTTPD_SEND_PUMP::FinishDecision finish = {
                D2B_HTTPD_SEND_PUMP::FinishResult::Stale,
                0,
            };
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId && _stopping)
            {
                finish = _pump.finishOrderlyStream(owner.generation, token);
                matched = finish.result == D2B_HTTPD_SEND_PUMP::FinishResult::Idle;
            }
            const std::uint32_t encoderHighWater = _encoder_stack_high_water;
            const std::uint32_t txHighWater = _tx_stack_high_water;
            portEXIT_CRITICAL(&_lock);
            if (!matched)
                return false;

            D2B_PRODUCER::Abort(streamId);
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId)
            {
                _stream_active = false;
                _stopping = false;
                _stream_id = 0;
                _output.clear();
                _stopped_response_size = 0;
            }
            portEXIT_CRITICAL(&_lock);
            D2B_RUNTIME_EVIDENCE::LogStreamStopCompleted(owner,
                                                         streamId,
                                                         GetSnapshot(),
                                                         D2B_PRODUCER::GetSnapshot());
            ESP_LOGI(kTag,
                     "stream stopped; stack high-water encoder=%lu tx=%lu bytes",
                     static_cast<unsigned long>(encoderHighWater),
                     static_cast<unsigned long>(txHighWater));
            return true;
        }

        void RequestPump(const OwnerKey& owner)
        {
            if (_send_mutex == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
                return;

            D2B_HTTPD_SEND_PUMP::ScheduleDecision decision = {D2B_HTTPD_SEND_PUMP::ScheduleResult::Inactive, 0};
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner) && _stream_active)
                decision = _pump.request(owner.generation);
            portEXIT_CRITICAL(&_lock);

            if (decision.result == D2B_HTTPD_SEND_PUMP::ScheduleResult::Accepted)
            {
                D2B_RUNTIME_EVIDENCE::LogPumpScheduleAccepted(owner, GetSnapshot(), D2B_PRODUCER::GetSnapshot());
                const esp_err_t queued = httpd_queue_work(owner.server,
                                                          PumpWork,
                                                          reinterpret_cast<void*>(decision.token));
                if (queued != ESP_OK)
                {
                    portENTER_CRITICAL(&_lock);
                    (void)_pump.reject(owner.generation, decision.token);
                    portEXIT_CRITICAL(&_lock);
                    D2B_RUNTIME_EVIDENCE::LogPumpQueueRejected(owner,
                                                               GetSnapshot(),
                                                               D2B_PRODUCER::GetSnapshot());
                }
            }
            else if (decision.result == D2B_HTTPD_SEND_PUMP::ScheduleResult::Coalesced)
            {
                D2B_RUNTIME_EVIDENCE::LogPumpScheduleCoalesced(owner, GetSnapshot(), D2B_PRODUCER::GetSnapshot());
            }
            xSemaphoreGive(_send_mutex);
        }

        void PumpWork(void* argument)
        {
            const std::uintptr_t token = reinterpret_cast<std::uintptr_t>(argument);
            if (token == 0 || _send_mutex == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
                return;

            OwnerKey owner = {};
            std::uint32_t streamId = 0;
            bool began = false;
            portENTER_CRITICAL(&_lock);
            owner = _owner;
            if (_owner_open && _stream_active && owner.generation != 0 &&
                _pump.begin(owner.generation, token) == D2B_HTTPD_SEND_PUMP::BeginResult::Began)
            {
                streamId = _stream_id;
                began = true;
            }
            portEXIT_CRITICAL(&_lock);

            if (!began)
            {
                D2B_RUNTIME_EVIDENCE::LogPumpStale(owner, GetSnapshot(), D2B_PRODUCER::GetSnapshot());
                xSemaphoreGive(_send_mutex);
                return;
            }

            D2B_RUNTIME_EVIDENCE::LogPumpCallbackBegin(owner, GetSnapshot(), D2B_PRODUCER::GetSnapshot());

            bool failed = false;
            bool orderlyCompleted = false;
            unsigned drained = 0;
            auto sendFrame = [&](httpd_ws_type_t type, std::uint8_t* payload, std::size_t size) -> esp_err_t
            {
                if (httpd_ws_get_fd_info(owner.server, owner.socket) != HTTPD_WS_CLIENT_WEBSOCKET)
                    return ESP_FAIL;
                httpd_ws_frame_t frame = {};
                frame.final = true;
                frame.type = type;
                frame.payload = payload;
                frame.len = size;
                return httpd_ws_send_frame_async(owner.server, owner.socket, &frame);
            };

            while (drained < kPumpFramesPerCallback)
            {
                D2B::OutputFrame queued = {};
                std::uint8_t pendingFlags = 0;
                bool firstFrame = false;
                std::uint64_t expectedSequence = 0;
                portENTER_CRITICAL(&_lock);
                const bool available = OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId &&
                                       _output.popForTransport(queued, pendingFlags);
                if (available)
                {
                    firstFrame = _first_transport_frame;
                    expectedSequence = _expected_sequence;
                }
                portEXIT_CRITICAL(&_lock);
                if (!available)
                    break;

                std::uint8_t prepared[D2B::kSingleViFrameSize];
                std::uint64_t nextSequence = 0;
                const bool preparedOk = D2B::PrepareOutputFrame(queued,
                                                                 pendingFlags,
                                                                 firstFrame,
                                                                 expectedSequence,
                                                                 prepared,
                                                                 sizeof(prepared),
                                                                 nextSequence);
                if (!preparedOk)
                {
                    FailStream(owner, streamId, true, true);
                    failed = true;
                    break;
                }
                if (sendFrame(HTTPD_WS_TYPE_BINARY, prepared, sizeof(prepared)) != ESP_OK)
                {
                    FailStream(owner, streamId, false, true);
                    failed = true;
                    break;
                }

                portENTER_CRITICAL(&_lock);
                if (OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId)
                {
                    _first_transport_frame = false;
                    _expected_sequence = nextSequence;
                }
                portEXIT_CRITICAL(&_lock);
                ++drained;
            }

            if (!failed && ReadyForStreamEnd())
            {
                const D2B_PRODUCER::Snapshot producer = D2B_PRODUCER::GetSnapshot();
                char stoppedResponse[kMaximumStoppedResponseSize] = {};
                std::size_t stoppedResponseSize = 0;
                std::uint64_t endTimestampUs = 0;
                portENTER_CRITICAL(&_lock);
                owner = _owner;
                streamId = _stream_id;
                endTimestampUs = _end_timestamp_us;
                stoppedResponseSize = _stopped_response_size;
                std::memcpy(stoppedResponse, _stopped_response, stoppedResponseSize);
                portEXIT_CRITICAL(&_lock);

                std::uint8_t endFrame[D2B::kEnvelopeSize];
                const D2B::FrameWriteResult end = D2B::WriteStreamEndFrame(endFrame,
                                                                            sizeof(endFrame),
                                                                            streamId,
                                                                            producer.nextSequence,
                                                                            endTimestampUs);
                if (!end.ok() || sendFrame(HTTPD_WS_TYPE_BINARY, endFrame, end.size) != ESP_OK ||
                    sendFrame(HTTPD_WS_TYPE_TEXT,
                              reinterpret_cast<std::uint8_t*>(stoppedResponse),
                              stoppedResponseSize) != ESP_OK)
                {
                    FailStream(owner, streamId, !end.ok(), true);
                    failed = true;
                }
                else
                {
                    orderlyCompleted = CompleteOrderlyStop(owner, streamId, token);
                    if (!orderlyCompleted)
                        failed = true;
                }
            }

            if (!failed && !orderlyCompleted)
            {
                D2B_HTTPD_SEND_PUMP::FinishDecision finish = {D2B_HTTPD_SEND_PUMP::FinishResult::Stale, 0};
                bool workRemains = false;
                const D2B_PRODUCER::Snapshot producer = D2B_PRODUCER::GetSnapshot();
                portENTER_CRITICAL(&_lock);
                if (OwnerMatchesLocked(owner))
                {
                    workRemains = _stream_active && _stream_id == streamId &&
                                  (_output.queuedCount() != 0 || ReadyForStreamEndLocked(producer));
                    finish = _pump.finish(owner.generation, token, workRemains);
                }
                portEXIT_CRITICAL(&_lock);

                if (finish.result == D2B_HTTPD_SEND_PUMP::FinishResult::Rescheduled)
                {
                    const esp_err_t queued = httpd_queue_work(owner.server,
                                                              PumpWork,
                                                              reinterpret_cast<void*>(finish.token));
                    if (queued != ESP_OK)
                    {
                        portENTER_CRITICAL(&_lock);
                        (void)_pump.reject(owner.generation, finish.token);
                        portEXIT_CRITICAL(&_lock);
                        D2B_RUNTIME_EVIDENCE::LogPumpQueueRejected(owner,
                                                                   GetSnapshot(),
                                                                   D2B_PRODUCER::GetSnapshot());
                    }
                }
            }
            if (began)
                D2B_RUNTIME_EVIDENCE::LogPumpCallbackEnd(owner, GetSnapshot(), D2B_PRODUCER::GetSnapshot());
            xSemaphoreGive(_send_mutex);
        }

        void TxTask(void*)
        {
            while (true)
            {
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
                UpdateHighWater(_tx_stack_high_water);
                EmitPendingEvidence();
                EmitTxEvidence();
                OwnerKey owner = {};
                portENTER_CRITICAL(&_lock);
                if (_owner_open && _stream_active)
                    owner = _owner;
                portEXIT_CRITICAL(&_lock);
                if (owner.generation != 0)
                    RequestPump(owner);
                UpdateHighWater(_tx_stack_high_water);
            }
        }
    } // namespace

    bool Initialize()
    {
        portENTER_CRITICAL(&_lock);
        if (_initialized)
        {
            const bool healthy = !_task_fault;
            portEXIT_CRITICAL(&_lock);
            return healthy;
        }
        portEXIT_CRITICAL(&_lock);

        _send_mutex = xSemaphoreCreateMutexStatic(&_send_mutex_storage);
        _tx_task = xTaskCreateStatic(TxTask,
                                     "d2b-tx",
                                     kTxStackBytes,
                                     nullptr,
                                     1,
                                     _tx_stack,
                                     &_tx_task_storage);
        _encoder_task = xTaskCreateStatic(EncoderTask,
                                          "d2b-enc",
                                          kEncoderStackBytes,
                                          nullptr,
                                          1,
                                          _encoder_stack,
                                          &_encoder_task_storage);
        const bool initialized = _send_mutex != nullptr && _tx_task != nullptr && _encoder_task != nullptr;
        if (initialized)
            D2B_PRODUCER::SetConsumerTask(_encoder_task);

        portENTER_CRITICAL(&_lock);
        _initialized = initialized;
        _task_fault = !initialized;
        portEXIT_CRITICAL(&_lock);
        return initialized;
    }

    bool CommitServerRunning(httpd_handle_t server, std::uint32_t generation)
    {
        if (_send_mutex == nullptr || server == nullptr || generation == 0 ||
            xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
            return false;

        portENTER_CRITICAL(&_lock);
        const bool committed = _initialized && !_task_fault &&
                               _lifecycle.beginRunning(HandleKey(server), generation);
        portEXIT_CRITICAL(&_lock);
        xSemaphoreGive(_send_mutex);
        return committed;
    }

    bool Open(const OwnerKey& owner)
    {
        if (_send_mutex == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
            return false;
        portENTER_CRITICAL(&_lock);
        bool accepted = _initialized && !_task_fault && !_owner_open && owner.server != nullptr &&
                        owner.socket >= 0 && owner.generation != 0 &&
                        LifecycleAcceptingLocked(owner.server);
        if (accepted)
            accepted = _pump.activate(owner.generation);
        if (accepted)
        {
            _owner = owner;
            _owner_open = true;
            _stream_active = false;
            _stopping = false;
            _stream_id = 0;
            _output.clear();
        }
        portEXIT_CRITICAL(&_lock);
        xSemaphoreGive(_send_mutex);
        return accepted;
    }

    void Close(const OwnerKey& owner, RUNTIME_EVIDENCE::Reason reason)
    {
        if (_send_mutex == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
            return;
        std::uint32_t streamId = 0;
        OwnerKey abruptOwner = {};
        bool matched = false;
        portENTER_CRITICAL(&_lock);
        if (OwnerMatchesLocked(owner))
        {
            (void)_pump.invalidate(owner.generation);
            streamId = _stream_id;
            if (streamId != 0 && _stream_active)
                abruptOwner = _owner;
            matched = true;
        }
        portEXIT_CRITICAL(&_lock);
        if (streamId != 0)
            D2B_PRODUCER::Abort(streamId);
        if (matched)
        {
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner))
            {
                OwnerKey ignoredOwner = {};
                std::uint32_t ignoredStreamId = 0;
                ClearPipelineLocked(ignoredStreamId, ignoredOwner);
            }
            portEXIT_CRITICAL(&_lock);
        }
        if (abruptOwner.generation != 0)
            D2B_RUNTIME_EVIDENCE::LogStreamAbrupt(abruptOwner,
                                                  streamId,
                                                  reason,
                                                  GetSnapshot(),
                                                  D2B_PRODUCER::GetSnapshot());
        WakeTasks();
        xSemaphoreGive(_send_mutex);
    }

    void StopServer(httpd_handle_t server, RUNTIME_EVIDENCE::Reason reason)
    {
        if (_send_mutex == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
            return;

        OwnerKey owner = {};
        D2B_HTTPD_LIFECYCLE::Snapshot lifecycle = {};
        portENTER_CRITICAL(&_lock);
        const std::uintptr_t key = HandleKey(server);
        lifecycle = _lifecycle.snapshot();
        const bool currentServer = lifecycle.handleKey == key && lifecycle.generation != 0;
        if (currentServer)
            (void)_lifecycle.beginStop(key, lifecycle.generation);

        std::uint32_t streamId = 0;
        OwnerKey abruptOwner = {};
        bool matched = false;
        if (currentServer && _owner_open && _owner.server == server)
        {
            owner = _owner;
            (void)_pump.invalidate(owner.generation);
            streamId = _stream_id;
            if (streamId != 0 && _stream_active)
                abruptOwner = _owner;
            matched = true;
        }
        portEXIT_CRITICAL(&_lock);

        if (streamId != 0)
            D2B_PRODUCER::Abort(streamId);
        if (matched)
        {
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner))
            {
                OwnerKey ignoredOwner = {};
                std::uint32_t ignoredStreamId = 0;
                ClearPipelineLocked(ignoredStreamId, ignoredOwner);
            }
            portEXIT_CRITICAL(&_lock);
        }
        if (abruptOwner.generation != 0)
            D2B_RUNTIME_EVIDENCE::LogStreamAbrupt(abruptOwner,
                                                  streamId,
                                                  reason,
                                                  GetSnapshot(),
                                                  D2B_PRODUCER::GetSnapshot());
        WakeTasks();
        xSemaphoreGive(_send_mutex);
    }

    void QuiesceSends()
    {
        if (_send_mutex != nullptr && xSemaphoreTake(_send_mutex, portMAX_DELAY) == pdTRUE)
            xSemaphoreGive(_send_mutex);
    }

    bool StartStream(const OwnerKey& owner, std::uint32_t streamId)
    {
        return StartStream(owner, streamId, nullptr, nullptr);
    }

    bool StartStream(const OwnerKey& owner,
                     std::uint32_t streamId,
                     StartPublicationCallback publish,
                     void* publishContext)
    {
        if (_send_mutex == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
            return false;
        portENTER_CRITICAL(&_lock);
        const bool accepted = OwnerMatchesLocked(owner) && !_stream_active && !_stopping && streamId != 0 &&
                              LifecycleAcceptingLocked(owner.server);
        if (accepted)
        {
            _stream_active = true;
            _stream_id = streamId;
            _first_transport_frame = true;
            _expected_sequence = 0;
            _end_timestamp_us = 0;
            _stopped_response_size = 0;
            _output.clear();
            _last_diag_producer_drops = 0;
            _last_diag_output_drops = 0;
            _diag_trend_limiter.Reset();
        }
        portEXIT_CRITICAL(&_lock);
        if (!accepted)
        {
            xSemaphoreGive(_send_mutex);
            return false;
        }

        D2B_PRODUCER::Start(streamId);
        const bool published = publish == nullptr || publish(publishContext);
        if (!published)
        {
            D2B_PRODUCER::Abort(streamId);
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId)
            {
                _stream_active = false;
                _stopping = false;
                _stream_id = 0;
                _output.clear();
            }
            portEXIT_CRITICAL(&_lock);
            xSemaphoreGive(_send_mutex);
            return false;
        }

        WakeTasks();
        xSemaphoreGive(_send_mutex);
        D2B_RUNTIME_EVIDENCE::LogStreamStart(owner, streamId, GetSnapshot(), D2B_PRODUCER::GetSnapshot());
        return true;
    }

    void MarkStopFailed(httpd_handle_t server)
    {
        if (_send_mutex == nullptr || server == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
            return;
        portENTER_CRITICAL(&_lock);
        const D2B_HTTPD_LIFECYCLE::Snapshot lifecycle = _lifecycle.snapshot();
        if (lifecycle.handleKey == HandleKey(server))
            (void)_lifecycle.stopFailure(lifecycle.handleKey, lifecycle.generation);
        portEXIT_CRITICAL(&_lock);
        xSemaphoreGive(_send_mutex);
    }

    void MarkStopSucceeded(httpd_handle_t server)
    {
        if (_send_mutex == nullptr || server == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
            return;
        portENTER_CRITICAL(&_lock);
        const D2B_HTTPD_LIFECYCLE::Snapshot lifecycle = _lifecycle.snapshot();
        if (lifecycle.handleKey == HandleKey(server))
            (void)_lifecycle.stopSuccess(lifecycle.handleKey, lifecycle.generation);
        portEXIT_CRITICAL(&_lock);
        xSemaphoreGive(_send_mutex);
    }

    bool RequestOrderlyStop(const OwnerKey& owner,
                            std::uint32_t streamId,
                            const char* stoppedResponse,
                            std::size_t stoppedResponseSize)
    {
        if (stoppedResponse == nullptr || stoppedResponseSize == 0 || stoppedResponseSize > sizeof(_stopped_response))
            return false;
        const std::uint64_t recognizedAt = static_cast<std::uint64_t>(esp_timer_get_time());

        if (_send_mutex == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
            return false;

        portENTER_CRITICAL(&_lock);
        bool accepted = OwnerMatchesLocked(owner) && _stream_active && !_stopping && _stream_id == streamId &&
                        LifecycleAcceptingLocked(owner.server);
        portEXIT_CRITICAL(&_lock);
        if (accepted)
            accepted = D2B_PRODUCER::Freeze(streamId);

        portENTER_CRITICAL(&_lock);
        accepted = accepted && OwnerMatchesLocked(owner) && _stream_active && !_stopping && _stream_id == streamId;
        if (accepted)
        {
            _stopping = true;
            _end_timestamp_us = recognizedAt;
            std::memcpy(_stopped_response, stoppedResponse, stoppedResponseSize);
            _stopped_response_size = stoppedResponseSize;
        }
        portEXIT_CRITICAL(&_lock);
        if (!accepted)
        {
            D2B_PRODUCER::Abort(streamId);
            xSemaphoreGive(_send_mutex);
            return false;
        }
        D2B_RUNTIME_EVIDENCE::LogStreamStopAccepted(owner,
                                                    streamId,
                                                    GetSnapshot(),
                                                    D2B_PRODUCER::GetSnapshot());
        WakeTasks();
        xSemaphoreGive(_send_mutex);
        return true;
    }

    bool StopPending(const OwnerKey& owner)
    {
        portENTER_CRITICAL(&_lock);
        const bool pending = OwnerMatchesLocked(owner) && _stopping;
        portEXIT_CRITICAL(&_lock);
        return pending;
    }

    Snapshot GetSnapshot()
    {
        portENTER_CRITICAL(&_lock);
        const D2B_HTTPD_LIFECYCLE::Snapshot lifecycle = _lifecycle.snapshot();
        const Snapshot snapshot = {
            _initialized,
            _owner_open,
            _stream_active,
            _stopping,
            _task_fault,
            _stream_id,
            lifecycle.accepting,
            lifecycle.handleKey,
            lifecycle.generation,
            static_cast<std::uint32_t>(_output.queuedCount()),
            _output.outputQueueDropCount(),
            _encoder_stack_high_water,
            _tx_stack_high_water,
        };
        portEXIT_CRITICAL(&_lock);
        return snapshot;
    }
} // namespace D2B_PIPELINE
