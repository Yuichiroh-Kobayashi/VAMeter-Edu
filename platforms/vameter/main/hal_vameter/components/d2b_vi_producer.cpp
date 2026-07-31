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
        portEXIT_CRITICAL(&_lock);
    }

    void Abort(std::uint32_t streamId)
    {
        portENTER_CRITICAL(&_lock);
        if (_active && _stream_id == streamId)
        {
            _active = false;
            _ring.clear();
            _stream_id = 0;
        }
        portEXIT_CRITICAL(&_lock);
    }

    void Tap(std::uint64_t timestampUs, std::uint32_t validMask, float voltageV, float currentA)
    {
        portENTER_CRITICAL(&_lock);
        if (_active && !_sequence_exhausted)
        {
            const D2B::AcquisitionSample sample =
                D2B::BuildAcquisitionSample(_next_sequence, timestampUs, validMask, voltageV, currentA);
            _ring.pushDropOldest(sample);
            _has_sample = true;
            _last_timestamp_us = timestampUs;
            if (_next_sequence == std::numeric_limits<std::uint64_t>::max())
                _sequence_exhausted = true;
            else
                ++_next_sequence;
        }
        portEXIT_CRITICAL(&_lock);
    }

    bool Pop(D2B::AcquisitionSample& sample, bool& producerOverflowBeforeSample, std::uint32_t& streamId)
    {
        portENTER_CRITICAL(&_lock);
        const bool available = _active && _ring.pop(sample, producerOverflowBeforeSample);
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
