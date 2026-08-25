#include "f15_observation_device.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace F15_OBS_DEVICE
{
    namespace
    {
        static const char* kTag = "f15-obs";
        static const std::size_t kOutputCapacity = 320;

        F15_OBS::ObservationRing<kQueueCapacity> _ring;
        StaticTask_t _drain_task_storage;
        StackType_t _drain_task_stack[kDrainTaskStackBytes];
        TaskHandle_t _drain_task = nullptr;
        std::uint64_t _next_sequence = 0;
        bool _sequence_exhausted = false;

        bool Pop(F15_OBS::ObservationRecord& record, std::uint64_t& droppedRecordCount)
        {
            const bool available = _ring.tryPop(record);
            droppedRecordCount = _ring.droppedRecordCount();
            return available;
        }

        void DrainTask(void*)
        {
            while (true)
            {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                F15_OBS::ObservationRecord record = {};
                std::uint64_t droppedRecordCount = 0;
                while (Pop(record, droppedRecordCount))
                {
                    char output[kOutputCapacity] = {};
                    std::size_t outputLength = 0;
                    if (F15_OBS::FormatObservation(record, droppedRecordCount, output, sizeof(output), outputLength))
                        ESP_LOGI(kTag, "%.*s", static_cast<int>(outputLength), output);
                    else
                        ESP_LOGE(kTag, "F15_OBS_FORMAT_ERROR seq=%llu", static_cast<unsigned long long>(record.sequence));
                }
            }
        }
    } // namespace

    bool Start()
    {
        if (_drain_task != nullptr)
            return true;

        _drain_task = xTaskCreateStatic(
            DrainTask, "f15-obs", kDrainTaskStackBytes, nullptr, tskIDLE_PRIORITY, _drain_task_stack, &_drain_task_storage);
        if (_drain_task == nullptr)
            return false;

        ESP_LOGI(kTag,
                 "F15_OBS_META v=1 queue_capacity=%u record_bytes=%u queue_storage_bytes=%u task_stack_bytes=%u",
                 static_cast<unsigned>(kQueueCapacity),
                 static_cast<unsigned>(sizeof(F15_OBS::ObservationRecord)),
                 static_cast<unsigned>(sizeof(_ring)),
                 static_cast<unsigned>(sizeof(_drain_task_stack)));
        return true;
    }

    void Publish(std::uint64_t timestampUs,
                 float shuntCurrentA,
                 float busVoltageV,
                 std::uint32_t validMask,
                 F15_OBS::CurrentRange currentRange,
                 bool currentReadSucceeded,
                 bool overflowReadSucceeded,
                 bool overflowAsserted)
    {
        TaskHandle_t drainTask = nullptr;
        bool queued = false;

        if (!_sequence_exhausted)
        {
            const F15_OBS::ObservationRecord record = {
                _next_sequence,
                timestampUs,
                shuntCurrentA,
                busVoltageV,
                validMask,
                currentRange,
                currentReadSucceeded,
                overflowReadSucceeded,
                overflowAsserted,
            };
            queued = _ring.tryPush(record);
            if (_next_sequence == std::numeric_limits<std::uint64_t>::max())
                _sequence_exhausted = true;
            else
                ++_next_sequence;
        }
        drainTask = _drain_task;

        if (queued && drainTask != nullptr)
            xTaskNotifyGive(drainTask);
    }
} // namespace F15_OBS_DEVICE
