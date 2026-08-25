#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace SIGNED_CURRENT_OBS
{
    enum class CurrentRange : std::uint8_t
    {
        Low = 0,
        High = 1,
    };

    struct ObservationRecord
    {
        std::uint64_t sequence;
        std::uint64_t timestampUs;
        float shuntCurrentA;
        float busVoltageV;
        std::uint32_t validMask;
        CurrentRange currentRange;
        bool currentReadSucceeded;
        bool overflowReadSucceeded;
        bool overflowAsserted;
    };

    const char* CurrentRangeToken(CurrentRange range);

    bool FormatObservation(const ObservationRecord& record,
                           std::uint64_t droppedRecordCount,
                           char* output,
                           std::size_t outputCapacity,
                           std::size_t& outputLength);

    template <std::size_t Capacity>
    class ObservationRing
    {
        static_assert(Capacity > 0, "signed-current observation ring capacity must be nonzero");
        static_assert(Capacity <= std::numeric_limits<std::uint32_t>::max(),
                      "signed-current observation ring capacity exceeds the index domain");

    public:
        // This bounded ring is single-producer/single-consumer. The producer owns
        // _tail and the drop counter; the consumer owns _head.
        ObservationRing() : _head(0), _tail(0), _droppedRecordCount(0) {}

        bool tryPush(const ObservationRecord& record)
        {
            const std::uint32_t tail = _tail.load(std::memory_order_relaxed);
            const std::uint32_t head = _head.load(std::memory_order_acquire);
            if (static_cast<std::uint32_t>(tail - head) == Capacity)
            {
                const std::uint32_t dropped = _droppedRecordCount.load(std::memory_order_relaxed);
                if (dropped != std::numeric_limits<std::uint32_t>::max())
                    _droppedRecordCount.store(dropped + 1, std::memory_order_relaxed);
                return false;
            }

            _records[tail % Capacity] = record;
            _tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        bool tryPop(ObservationRecord& record)
        {
            const std::uint32_t head = _head.load(std::memory_order_relaxed);
            const std::uint32_t tail = _tail.load(std::memory_order_acquire);
            if (head == tail)
                return false;

            record = _records[head % Capacity];
            _head.store(head + 1, std::memory_order_release);
            return true;
        }

        std::size_t queuedCount() const
        {
            const std::uint32_t tail = _tail.load(std::memory_order_acquire);
            const std::uint32_t head = _head.load(std::memory_order_acquire);
            return static_cast<std::uint32_t>(tail - head);
        }
        std::uint64_t droppedRecordCount() const { return _droppedRecordCount.load(std::memory_order_relaxed); }

    private:
        ObservationRecord _records[Capacity];
        std::atomic<std::uint32_t> _head;
        std::atomic<std::uint32_t> _tail;
        std::atomic<std::uint32_t> _droppedRecordCount;
    };
} // namespace SIGNED_CURRENT_OBS
