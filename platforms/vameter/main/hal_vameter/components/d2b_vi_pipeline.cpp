#include "d2b_vi_pipeline.h"

#include "d2b_vi_producer.h"
#include "libs/d2b_vi/d2b_frame_writer.h"
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

        void UpdateHighWater(std::uint32_t& stored)
        {
            const std::uint32_t current = static_cast<std::uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
            portENTER_CRITICAL(&_lock);
            if (current < stored)
                stored = current;
            portEXIT_CRITICAL(&_lock);
        }

        esp_err_t SendFrame(const OwnerKey& owner, httpd_ws_type_t type, std::uint8_t* payload, std::size_t size)
        {
            if (_send_mutex == nullptr || xSemaphoreTake(_send_mutex, portMAX_DELAY) != pdTRUE)
                return ESP_FAIL;

            portENTER_CRITICAL(&_lock);
            const bool matched = OwnerMatchesLocked(owner);
            portEXIT_CRITICAL(&_lock);

            esp_err_t result = ESP_FAIL;
            if (matched && httpd_ws_get_fd_info(owner.server, owner.socket) == HTTPD_WS_CLIENT_WEBSOCKET)
            {
                httpd_ws_frame_t frame = {};
                frame.final = true;
                frame.type = type;
                frame.payload = payload;
                frame.len = size;
                result = httpd_ws_send_frame_async(owner.server, owner.socket, &frame);
            }
            xSemaphoreGive(_send_mutex);
            return result;
        }

        void WakeTasks()
        {
            if (_encoder_task != nullptr)
                xTaskNotifyGive(_encoder_task);
            if (_tx_task != nullptr)
                xTaskNotifyGive(_tx_task);
        }

        void FailStream(const OwnerKey& owner, std::uint32_t streamId, bool taskFault)
        {
            bool matched = false;
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId)
            {
                _stream_active = false;
                _stopping = false;
                _stream_id = 0;
                _output.clear();
                _task_fault = _task_fault || taskFault;
                _owner = {};
                _owner_open = false;
                matched = true;
            }
            portEXIT_CRITICAL(&_lock);
            if (matched)
            {
                D2B_PRODUCER::Abort(streamId);
                if (taskFault)
                    ESP_LOGE(kTag, "internal stream pipeline failure; closing owner generation=%lu",
                             static_cast<unsigned long>(owner.generation));
                else
                    ESP_LOGW(kTag, "WebSocket send failed; closing owner generation=%lu",
                             static_cast<unsigned long>(owner.generation));
                WakeTasks();
                const esp_err_t closeResult = httpd_sess_trigger_close(owner.server, owner.socket);
                if (closeResult != ESP_OK)
                    ESP_LOGE(kTag, "failed to schedule owner close: %s", esp_err_to_name(closeResult));
            }
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
                        _output.pushDropOldest(frame);
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
                        FailStream(owner, streamId, true);
                        break;
                    }
                    if (accepted && _tx_task != nullptr)
                        xTaskNotifyGive(_tx_task);
                }
                UpdateHighWater(_encoder_stack_high_water);
            }
        }

        bool ReadyForStreamEnd()
        {
            const D2B_PRODUCER::Snapshot producer = D2B_PRODUCER::GetSnapshot();
            portENTER_CRITICAL(&_lock);
            const bool ready = _stopping && _stream_active && !_encoder_busy &&
                               producer.streamId == _stream_id && producer.queuedSampleCount == 0 &&
                               _output.queuedCount() == 0;
            portEXIT_CRITICAL(&_lock);
            return ready;
        }

        void CompleteOrderlyStop(const OwnerKey& owner, std::uint32_t streamId)
        {
            portENTER_CRITICAL(&_lock);
            if (OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId && _stopping)
            {
                _stream_active = false;
                _stopping = false;
                _stream_id = 0;
                _output.clear();
                _stopped_response_size = 0;
            }
            const std::uint32_t encoderHighWater = _encoder_stack_high_water;
            const std::uint32_t txHighWater = _tx_stack_high_water;
            portEXIT_CRITICAL(&_lock);
            D2B_PRODUCER::Abort(streamId);
            ESP_LOGI(kTag,
                     "stream stopped; stack high-water encoder=%lu tx=%lu bytes",
                     static_cast<unsigned long>(encoderHighWater),
                     static_cast<unsigned long>(txHighWater));
        }

        void TxTask(void*)
        {
            while (true)
            {
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
                UpdateHighWater(_tx_stack_high_water);
                while (true)
                {
                    D2B::OutputFrame queued = {};
                    std::uint8_t pendingFlags = 0;
                    OwnerKey owner = {};
                    std::uint32_t streamId = 0;
                    bool firstFrame = false;
                    std::uint64_t expectedSequence = 0;

                    portENTER_CRITICAL(&_lock);
                    const bool available = _owner_open && _stream_active &&
                                           _output.popForTransport(queued, pendingFlags);
                    if (available)
                    {
                        owner = _owner;
                        streamId = _stream_id;
                        firstFrame = _first_transport_frame;
                        expectedSequence = _expected_sequence;
                    }
                    portEXIT_CRITICAL(&_lock);

                    if (available)
                    {
                        std::uint8_t prepared[D2B::kSingleViFrameSize];
                        std::uint64_t nextSequence = 0;
                        if (!D2B::PrepareOutputFrame(queued,
                                                     pendingFlags,
                                                     firstFrame,
                                                     expectedSequence,
                                                     prepared,
                                                     sizeof(prepared),
                                                     nextSequence))
                        {
                            FailStream(owner, streamId, true);
                            break;
                        }
                        if (SendFrame(owner, HTTPD_WS_TYPE_BINARY, prepared, sizeof(prepared)) != ESP_OK)
                        {
                            FailStream(owner, streamId, false);
                            break;
                        }

                        portENTER_CRITICAL(&_lock);
                        if (OwnerMatchesLocked(owner) && _stream_active && _stream_id == streamId)
                        {
                            _first_transport_frame = false;
                            _expected_sequence = nextSequence;
                        }
                        portEXIT_CRITICAL(&_lock);
                        continue;
                    }

                    if (!ReadyForStreamEnd())
                        break;

                    const D2B_PRODUCER::Snapshot producer = D2B_PRODUCER::GetSnapshot();
                    char stoppedResponse[kMaximumStoppedResponseSize];
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
                    if (!end.ok())
                    {
                        FailStream(owner, streamId, true);
                        break;
                    }
                    if (SendFrame(owner, HTTPD_WS_TYPE_BINARY, endFrame, end.size) != ESP_OK ||
                        SendFrame(owner,
                                  HTTPD_WS_TYPE_TEXT,
                                  reinterpret_cast<std::uint8_t*>(stoppedResponse),
                                  stoppedResponseSize) != ESP_OK)
                    {
                        FailStream(owner, streamId, false);
                        break;
                    }
                    CompleteOrderlyStop(owner, streamId);
                    break;
                }
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

    bool Open(const OwnerKey& owner)
    {
        portENTER_CRITICAL(&_lock);
        const bool accepted = _initialized && !_task_fault && !_owner_open && owner.server != nullptr &&
                              owner.socket >= 0 && owner.generation != 0;
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
        return accepted;
    }

    void Close(const OwnerKey& owner)
    {
        std::uint32_t streamId = 0;
        portENTER_CRITICAL(&_lock);
        if (OwnerMatchesLocked(owner))
        {
            streamId = _stream_id;
            _owner = {};
            _owner_open = false;
            _stream_active = false;
            _stopping = false;
            _stream_id = 0;
            _output.clear();
            _stopped_response_size = 0;
        }
        portEXIT_CRITICAL(&_lock);
        if (streamId != 0)
            D2B_PRODUCER::Abort(streamId);
        WakeTasks();
    }

    void StopServer(httpd_handle_t server)
    {
        OwnerKey owner = {};
        portENTER_CRITICAL(&_lock);
        if (_owner_open && _owner.server == server)
            owner = _owner;
        portEXIT_CRITICAL(&_lock);
        if (owner.server != nullptr)
            Close(owner);
    }

    void QuiesceSends()
    {
        if (_send_mutex != nullptr && xSemaphoreTake(_send_mutex, portMAX_DELAY) == pdTRUE)
            xSemaphoreGive(_send_mutex);
    }

    esp_err_t SendText(const OwnerKey& owner, const char* payload, std::size_t size)
    {
        if (payload == nullptr || size == 0)
            return ESP_ERR_INVALID_ARG;
        return SendFrame(owner,
                         HTTPD_WS_TYPE_TEXT,
                         reinterpret_cast<std::uint8_t*>(const_cast<char*>(payload)),
                         size);
    }

    bool StartStream(const OwnerKey& owner, std::uint32_t streamId)
    {
        portENTER_CRITICAL(&_lock);
        const bool accepted = OwnerMatchesLocked(owner) && !_stream_active && !_stopping && streamId != 0;
        if (accepted)
        {
            _stream_active = true;
            _stream_id = streamId;
            _first_transport_frame = true;
            _expected_sequence = 0;
            _end_timestamp_us = 0;
            _stopped_response_size = 0;
            _output.clear();
        }
        portEXIT_CRITICAL(&_lock);
        if (!accepted)
            return false;
        D2B_PRODUCER::Start(streamId);
        WakeTasks();
        return true;
    }

    bool RequestOrderlyStop(const OwnerKey& owner,
                            std::uint32_t streamId,
                            const char* stoppedResponse,
                            std::size_t stoppedResponseSize)
    {
        if (stoppedResponse == nullptr || stoppedResponseSize == 0 ||
            stoppedResponseSize > sizeof(_stopped_response) || !D2B_PRODUCER::Freeze(streamId))
            return false;
        const std::uint64_t recognizedAt = static_cast<std::uint64_t>(esp_timer_get_time());

        portENTER_CRITICAL(&_lock);
        const bool accepted = OwnerMatchesLocked(owner) && _stream_active && !_stopping && _stream_id == streamId;
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
            return false;
        }
        WakeTasks();
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
        const Snapshot snapshot = {
            _initialized,
            _owner_open,
            _stream_active,
            _stopping,
            _task_fault,
            _stream_id,
            static_cast<std::uint32_t>(_output.queuedCount()),
            _output.outputQueueDropCount(),
            _encoder_stack_high_water,
            _tx_stack_high_water,
        };
        portEXIT_CRITICAL(&_lock);
        return snapshot;
    }
} // namespace D2B_PIPELINE
