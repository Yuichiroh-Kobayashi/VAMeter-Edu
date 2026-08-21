#include "d2b_vi_producer.h"

#include <freertos/FreeRTOS.h>

#include <limits>

namespace D2B_PRODUCER
{
    namespace
    {
        portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
        D2B::AcquisitionRing _ring;
        bool _active = false;
        bool _has_sample = false;
        bool _sequence_exhausted = false;
        std::uint32_t _stream_id = 0;
        std::uint64_t _next_sequence = 0;
        std::uint64_t _last_timestamp_us = 0;
        TaskHandle_t _consumer_task = nullptr;

        void NotifyConsumer(TaskHandle_t consumer)
        {
            if (consumer != nullptr)
                xTaskNotifyGive(consumer);
        }
    } // namespace

    void Start(std::uint32_t streamId)
    {
        portENTER_CRITICAL(&_lock);
        _ring.clear();
        _active = streamId != 0;
        _has_sample = false;
        _sequence_exhausted = false;
        _stream_id = streamId;
        _next_sequence = 0;
        _last_timestamp_us = 0;
        const TaskHandle_t consumer = _consumer_task;
        portEXIT_CRITICAL(&_lock);
        NotifyConsumer(consumer);
    }

    bool Freeze(std::uint32_t streamId)
    {
        portENTER_CRITICAL(&_lock);
        const bool matched = _active && _stream_id == streamId;
        if (matched)
            _active = false;
        const TaskHandle_t consumer = _consumer_task;
        portEXIT_CRITICAL(&_lock);
        if (matched)
            NotifyConsumer(consumer);
        return matched;
    }

    void Abort(std::uint32_t streamId)
    {
        portENTER_CRITICAL(&_lock);
        if (_stream_id == streamId)
        {
            _active = false;
            _ring.clear();
            _stream_id = 0;
        }
        const TaskHandle_t consumer = _consumer_task;
        portEXIT_CRITICAL(&_lock);
        NotifyConsumer(consumer);
    }

    void SetConsumerTask(TaskHandle_t task)
    {
        portENTER_CRITICAL(&_lock);
        _consumer_task = task;
        portEXIT_CRITICAL(&_lock);
    }

    void Tap(std::uint64_t timestampUs, std::uint32_t validMask, float voltageV, float currentA)
    {
        bool queued = false;
        TaskHandle_t consumer = nullptr;
        portENTER_CRITICAL(&_lock);
        if (_active && !_sequence_exhausted)
        {
            const D2B::AcquisitionSample sample =
                D2B::BuildAcquisitionSample(_next_sequence, timestampUs, validMask, voltageV, currentA);
            _ring.pushDropOldest(sample);
            queued = true;
            _has_sample = true;
            _last_timestamp_us = timestampUs;
            if (_next_sequence == std::numeric_limits<std::uint64_t>::max())
                _sequence_exhausted = true;
            else
                ++_next_sequence;
        }
        consumer = _consumer_task;
        portEXIT_CRITICAL(&_lock);
        if (queued)
            NotifyConsumer(consumer);
    }

    bool Pop(D2B::AcquisitionSample& sample, bool& producerOverflowBeforeSample, std::uint32_t& streamId)
    {
        portENTER_CRITICAL(&_lock);
        const bool available = _stream_id != 0 && _ring.pop(sample, producerOverflowBeforeSample);
        streamId = available ? _stream_id : 0;
        portEXIT_CRITICAL(&_lock);
        return available;
    }

    Snapshot GetSnapshot()
    {
        portENTER_CRITICAL(&_lock);
        const Snapshot snapshot = {
            _active,
            _has_sample,
            _sequence_exhausted,
            _stream_id,
            _next_sequence,
            _last_timestamp_us,
            _ring.producerDropCount(),
            static_cast<std::uint32_t>(_ring.queuedCount()),
        };
        portEXIT_CRITICAL(&_lock);
        return snapshot;
    }
} // namespace D2B_PRODUCER
