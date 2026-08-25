#include "signed_current_observation_device.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include <cstddef>
#include <cstdint>
#include <limits>

#if defined(__GNUC__) && defined(CONFIG_IDF_TARGET_ESP32S3)
static_assert(__atomic_always_lock_free(sizeof(std::uint32_t), nullptr),
              "signed-current observation requires lock-free 32-bit atomics");
#endif

namespace SIGNED_CURRENT_OBS_DEVICE
{
    namespace
    {
        enum class Availability : std::uint8_t
        {
            NotStarted,
            Available,
            Unavailable,
        };

        static const char* kTag = "current-obs";
        static const std::size_t kOutputCapacity = 320;

        SIGNED_CURRENT_OBS::ObservationRing<kQueueCapacity> _ring;
        StaticTask_t _drain_task_storage;
        StackType_t _drain_task_stack[kDrainTaskStackBytes];
        TaskHandle_t _drain_task = nullptr;
        std::uint64_t _next_sequence = 0;
        bool _sequence_exhausted = false;
        Availability _availability = Availability::NotStarted;

        bool Pop(SIGNED_CURRENT_OBS::ObservationRecord& record, std::uint64_t& droppedRecordCount)
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

                SIGNED_CURRENT_OBS::ObservationRecord record = {};
                std::uint64_t droppedRecordCount = 0;
                while (Pop(record, droppedRecordCount))
                {
                    char output[kOutputCapacity] = {};
                    std::size_t outputLength = 0;
                    if (SIGNED_CURRENT_OBS::FormatObservation(record, droppedRecordCount, output, sizeof(output), outputLength))
                        ESP_LOGI(kTag, "%.*s", static_cast<int>(outputLength), output);
                    else
                        ESP_LOGE(
                            kTag, "SIGNED_CURRENT_OBS_FORMAT_ERROR seq=%llu", static_cast<unsigned long long>(record.sequence));
                }
            }
        }
    } // namespace

    bool Start()
    {
        if (_availability == Availability::Available)
            return true;
        if (_availability == Availability::Unavailable)
            return false;

        _drain_task = xTaskCreateStatic(
            DrainTask, "current-obs", kDrainTaskStackBytes, nullptr, tskIDLE_PRIORITY, _drain_task_stack, &_drain_task_storage);
        if (_drain_task == nullptr)
        {
            _availability = Availability::Unavailable;
            ESP_LOGE(kTag, "SIGNED_CURRENT_OBS_DRAIN_START_FAILED observation_available=0");
            return false;
        }

        _availability = Availability::Available;
        ESP_LOGI(kTag,
                 "SIGNED_CURRENT_OBS_META v=1 queue_capacity=%u record_bytes=%u queue_storage_bytes=%u "
                 "task_stack_bytes=%u",
                 static_cast<unsigned>(kQueueCapacity),
                 static_cast<unsigned>(sizeof(SIGNED_CURRENT_OBS::ObservationRecord)),
                 static_cast<unsigned>(sizeof(_ring)),
                 static_cast<unsigned>(sizeof(_drain_task_stack)));
        return true;
    }

    void Publish(std::uint64_t timestampUs,
                 float shuntCurrentA,
                 float busVoltageV,
                 std::uint32_t validMask,
                 SIGNED_CURRENT_OBS::CurrentRange currentRange,
                 bool currentReadSucceeded,
                 bool overflowReadSucceeded,
                 bool overflowAsserted)
    {
        if (_availability != Availability::Available)
            return;

        TaskHandle_t drainTask = nullptr;
        bool queued = false;

        if (!_sequence_exhausted)
        {
            const SIGNED_CURRENT_OBS::ObservationRecord record = {
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
} // namespace SIGNED_CURRENT_OBS_DEVICE
