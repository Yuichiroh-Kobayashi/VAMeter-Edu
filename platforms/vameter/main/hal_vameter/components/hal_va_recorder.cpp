/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */
#include "../hal_vameter.h"
#include "../hal_config.h"
#include "libs/local_csv_download/local_csv_download_name.h"
#include "libs/recorder_lifecycle/owned_task_resource.h"
#include <mooncake.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <new>
#include <memory>
#include <sys/stat.h>

POWER_MONITOR::PMData_t* _borrow_pm_data_daemon();
void _return_pm_data_daemon();

namespace
{
constexpr const char* kStoragePath = "/spiflash";
constexpr const char* kTempPath = "/spiflash/temp";
// Conservative headroom for temp chunks and the final CSV to coexist, plus FAT allocation/directory metadata.
constexpr uint64_t kRecorderMinimumFreeBytes = 64U * 1024U;
constexpr uint32_t kChunkSaveIntervalMs = 5000;
constexpr size_t kMergeBufferBytes = 8192;

enum RunningState_t
{
    daemon_state_preparing = 0,
    daemon_state_waiting,
    daemon_state_recording,
    daemon_state_wraping_up,
    daemon_state_finished,
    daemon_state_error,
};

struct RecorderDaemonStatus_t
{
    struct Data_t
    {
        char savePath[64] = {0};
        RunningState_t currentState = daemon_state_preparing;
        VA_RECORDER::Error_t error = VA_RECORDER::error_none;
    };

    Data_t data;
    RECORDER_LIFECYCLE::OwnedTaskResource<VA_RECORDER::TriggerBase> triggerOwner;
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    ~RecorderDaemonStatus_t() { if (mutex != nullptr) vSemaphoreDelete(mutex); }

    Data_t snapshot()
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        const Data_t copy = data;
        xSemaphoreGive(mutex);
        return copy;
    }
    void setState(RunningState_t state)
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        data.currentState = state;
        xSemaphoreGive(mutex);
    }
    bool acquireTrigger(std::unique_ptr<VA_RECORDER::TriggerBase> trigger)
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        const bool acquired = triggerOwner.acquire(std::move(trigger));
        xSemaphoreGive(mutex);
        return acquired;
    }
    VA_RECORDER::TriggerBase* trigger()
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        VA_RECORDER::TriggerBase* value = triggerOwner.get();
        xSemaphoreGive(mutex);
        return value;
    }
    void taskStarted()
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        triggerOwner.taskStarted();
        xSemaphoreGive(mutex);
    }
    void taskCreateFailed()
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        triggerOwner.taskCreateFailed();
        xSemaphoreGive(mutex);
    }
    void requestStop()
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        triggerOwner.requestStop();
        xSemaphoreGive(mutex);
    }
    bool stopRequested()
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        const bool value = triggerOwner.stopRequested();
        xSemaphoreGive(mutex);
        return value;
    }
    void finish(VA_RECORDER::Error_t value)
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        data.error = value;
        data.currentState = value == VA_RECORDER::error_none ? daemon_state_finished : daemon_state_error;
        triggerOwner.taskFinished();
        xSemaphoreGive(mutex);
    }
    bool releaseFinishedTrigger()
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
        const bool released = triggerOwner.releaseFinished();
        xSemaphoreGive(mutex);
        return released;
    }
};

RecorderDaemonStatus_t* g_recorder = nullptr;
VA_RECORDER::Error_t g_last_create_error = VA_RECORDER::error_none;

void logErrno(const char* operation, const char* path)
{
    const int value = errno;
    spdlog::error("{} {} failed: errno={} ({})", operation, path, value, strerror(value));
}

bool removeTempDirectory()
{
    DIR* dir = opendir(kTempPath);
    if (dir == nullptr)
    {
        if (errno == ENOENT) return true;
        logErrno("opendir", kTempPath);
        return false;
    }
    bool ok = true;
    while (dirent* entry = readdir(dir))
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char path[128] = {0};
        const int length = snprintf(path, sizeof(path), "%s/%s", kTempPath, entry->d_name);
        if (length <= 0 || static_cast<size_t>(length) >= sizeof(path) || remove(path) != 0)
        {
            logErrno("remove", path);
            ok = false;
        }
        esp_task_wdt_reset();
    }
    if (closedir(dir) != 0) { logErrno("closedir", kTempPath); ok = false; }
    if (rmdir(kTempPath) != 0) { logErrno("rmdir", kTempPath); ok = false; }
    return ok;
}

bool prepareTempDirectory()
{
    if (!removeTempDirectory()) return false;
    if (mkdir(kTempPath, S_IRWXU) != 0) { logErrno("mkdir", kTempPath); return false; }
    char probePath[64] = {0};
    snprintf(probePath, sizeof(probePath), "%s/1.csv", kTempPath);
    FILE* probe = fopen(probePath, "w");
    if (probe == nullptr) { logErrno("fopen", probePath); removeTempDirectory(); return false; }
    if (fclose(probe) != 0) { logErrno("fclose", probePath); removeTempDirectory(); return false; }
    if (remove(probePath) != 0) { logErrno("remove", probePath); removeTempDirectory(); return false; }
    return true;
}

void recorderTask(void* parameter)
{
    esp_task_wdt_add(nullptr);
    RecorderDaemonStatus_t* status = static_cast<RecorderDaemonStatus_t*>(parameter);
    status->taskStarted();
    const RecorderDaemonStatus_t::Data_t initial = status->snapshot();
    VA_RECORDER::TriggerBase* trigger = status->trigger();
    VA_RECORDER::PreTriggerBuffer* pretrigger = nullptr;
    FILE* chunk = nullptr;
    FILE* finalFile = nullptr;
    char* copyBuffer = nullptr;
    char chunkPath[64] = {0};
    uint16_t chunkCount = 1;
    bool finalCreated = false;
    bool completed = false;
    VA_RECORDER::Error_t error = VA_RECORDER::error_none;
    uint32_t recordingDurationMs = 0;

    if (trigger->preTriggerBufferSize() != 0)
    {
        pretrigger = new (std::nothrow) VA_RECORDER::PreTriggerBuffer;
        if (pretrigger == nullptr) { error = VA_RECORDER::error_allocation_failed; goto cleanup; }
        pretrigger->reSize(trigger->preTriggerBufferSize());
    }
    else spdlog::info("no pretrigger buffer");

    snprintf(chunkPath, sizeof(chunkPath), "%s/%u.csv", kTempPath, chunkCount);
    chunk = fopen(chunkPath, "w");
    if (chunk == nullptr) { logErrno("fopen", chunkPath); error = VA_RECORDER::error_open_chunk_failed; goto cleanup; }
    status->setState(daemon_state_waiting);

    while (!trigger->onCheck(pretrigger))
    {
        if (pretrigger != nullptr)
        {
            POWER_MONITOR::PMData_t* data = _borrow_pm_data_daemon();
            pretrigger->put(VA_RECORDER::RecordData_t(data->busVoltage, data->shuntCurrent));
            _return_pm_data_daemon();
        }
        if (status->stopRequested()) goto cleanup;
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(trigger->getSampleInterval()));
    }

    status->setState(daemon_state_recording);
    HAL::ResetPowerMonitorData();
    {
        const int64_t startedUs = esp_timer_get_time();
        uint32_t chunkStartedMs = 0;
        while (true)
        {
            POWER_MONITOR::PMData_t* data = _borrow_pm_data_daemon();
            const uint32_t elapsedMs = static_cast<uint32_t>((esp_timer_get_time() - startedUs) / 1000);
            const int result = fprintf(chunk, "%.4f,%.7f,%lu,,\n", data->busVoltage, data->shuntCurrent,
                                       static_cast<unsigned long>(elapsedMs));
            _return_pm_data_daemon();
            if (result < 0) { logErrno("fprintf", chunkPath); error = VA_RECORDER::error_write_chunk_failed; goto cleanup; }
            recordingDurationMs = static_cast<uint32_t>((esp_timer_get_time() - startedUs) / 1000);
            if (recordingDurationMs >= trigger->getRecordTime()) break;
            if (recordingDurationMs - chunkStartedMs >= kChunkSaveIntervalMs)
            {
                if (fclose(chunk) != 0) { chunk = nullptr; logErrno("fclose", chunkPath); error = VA_RECORDER::error_close_failed; goto cleanup; }
                chunk = nullptr;
                ++chunkCount;
                snprintf(chunkPath, sizeof(chunkPath), "%s/%u.csv", kTempPath, chunkCount);
                chunk = fopen(chunkPath, "w");
                if (chunk == nullptr) { logErrno("fopen", chunkPath); error = VA_RECORDER::error_open_chunk_failed; goto cleanup; }
                chunkStartedMs = recordingDurationMs;
            }
            if (status->stopRequested()) goto cleanup;
            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(trigger->getSampleInterval()));
        }
    }
    if (fclose(chunk) != 0) { chunk = nullptr; logErrno("fclose", chunkPath); error = VA_RECORDER::error_close_failed; goto cleanup; }
    chunk = nullptr;
    status->setState(daemon_state_wraping_up);

    finalFile = fopen(initial.savePath, "w");
    if (finalFile == nullptr) { logErrno("fopen", initial.savePath); error = VA_RECORDER::error_open_final_failed; goto cleanup; }
    finalCreated = true;
    if (fprintf(finalFile, "voltage,current,elapsed_ms,capacity,energy\n") < 0)
    { logErrno("fprintf", initial.savePath); error = VA_RECORDER::error_write_final_failed; goto cleanup; }
    {
        POWER_MONITOR::PMData_t* data = _borrow_pm_data_daemon();
        const int result = fprintf(finalFile, ",,%lu,%.7f,%.7f\n", static_cast<unsigned long>(recordingDurationMs),
                                   data->capacity, data->energy);
        _return_pm_data_daemon();
        if (result < 0) { logErrno("fprintf", initial.savePath); error = VA_RECORDER::error_write_final_failed; goto cleanup; }
    }
    if (pretrigger != nullptr)
    {
        bool writeOk = true;
        pretrigger->peekAll([&](const VA_RECORDER::RecordData_t& data) {
            if (writeOk && fprintf(finalFile, "%.4f,%.7f,0,,\n", data.voltage, data.current) < 0) writeOk = false;
        });
        if (!writeOk) { logErrno("fprintf", initial.savePath); error = VA_RECORDER::error_write_final_failed; goto cleanup; }
        delete pretrigger;
        pretrigger = nullptr;
    }

    copyBuffer = static_cast<char*>(malloc(kMergeBufferBytes));
    if (copyBuffer == nullptr) { spdlog::error("malloc {} failed", kMergeBufferBytes); error = VA_RECORDER::error_allocation_failed; goto cleanup; }
    for (uint16_t i = 1; i <= chunkCount; ++i)
    {
        snprintf(chunkPath, sizeof(chunkPath), "%s/%u.csv", kTempPath, i);
        chunk = fopen(chunkPath, "rb");
        if (chunk == nullptr) { logErrno("fopen", chunkPath); error = VA_RECORDER::error_open_chunk_failed; goto cleanup; }
        while (true)
        {
            const size_t readSize = fread(copyBuffer, 1, kMergeBufferBytes, chunk);
            if (readSize != 0 && fwrite(copyBuffer, 1, readSize, finalFile) != readSize)
            { logErrno("fwrite", initial.savePath); error = VA_RECORDER::error_write_final_failed; goto cleanup; }
            if (readSize < kMergeBufferBytes)
            {
                if (ferror(chunk)) { logErrno("fread", chunkPath); error = VA_RECORDER::error_write_final_failed; goto cleanup; }
                break;
            }
        }
        if (fclose(chunk) != 0) { chunk = nullptr; logErrno("fclose", chunkPath); error = VA_RECORDER::error_close_failed; goto cleanup; }
        chunk = nullptr;
        esp_task_wdt_reset();
    }
    if (fclose(finalFile) != 0) { finalFile = nullptr; logErrno("fclose", initial.savePath); error = VA_RECORDER::error_close_failed; goto cleanup; }
    finalFile = nullptr;
    if (!removeTempDirectory()) { error = VA_RECORDER::error_temp_prepare_failed; goto cleanup; }
    spdlog::info("done, saved at {}", initial.savePath);
    completed = true;

cleanup:
    if (chunk != nullptr && fclose(chunk) != 0)
    { logErrno("fclose", chunkPath); if (error == VA_RECORDER::error_none) error = VA_RECORDER::error_close_failed; }
    if (finalFile != nullptr && fclose(finalFile) != 0)
    { logErrno("fclose", initial.savePath); if (error == VA_RECORDER::error_none) error = VA_RECORDER::error_close_failed; }
    free(copyBuffer);
    delete pretrigger;
    if (!completed && !removeTempDirectory() && error == VA_RECORDER::error_none)
        error = VA_RECORDER::error_temp_prepare_failed;
    if (error != VA_RECORDER::error_none)
    {
        if (finalCreated && remove(initial.savePath) != 0 && errno != ENOENT) logErrno("remove", initial.savePath);
        status->finish(error);
    }
    else status->finish(VA_RECORDER::error_none);
    esp_task_wdt_delete(nullptr);
    vTaskDelete(nullptr);
}
} // namespace

bool HAL_VAMeter::creatVaRecorder(std::unique_ptr<VA_RECORDER::TriggerBase> trigger)
{
    g_last_create_error = VA_RECORDER::error_none;
    if (!trigger || g_recorder != nullptr)
    {
        spdlog::error("recorder already exists or trigger is null");
        return false;
    }
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
    const esp_err_t infoResult = esp_vfs_fat_info(kStoragePath, &totalBytes, &freeBytes);
    spdlog::info("record storage: total={} free={} required={}", totalBytes, freeBytes, kRecorderMinimumFreeBytes);
    if (infoResult != ESP_OK)
    {
        spdlog::error("esp_vfs_fat_info {} failed: {}", kStoragePath, esp_err_to_name(infoResult));
        g_last_create_error = VA_RECORDER::error_storage_info_failed;
        return false;
    }
    if (!LOCAL_CSV_DOWNLOAD::HasEnoughRecordingSpace(true, freeBytes, kRecorderMinimumFreeBytes))
    {
        spdlog::warn("recording start rejected: insufficient storage");
        g_last_create_error = VA_RECORDER::error_insufficient_space;
        return false;
    }
    if (!prepareTempDirectory())
    {
        g_last_create_error = VA_RECORDER::error_temp_prepare_failed;
        return false;
    }
    g_recorder = new (std::nothrow) RecorderDaemonStatus_t;
    if (g_recorder == nullptr || g_recorder->mutex == nullptr)
    {
        delete g_recorder;
        g_recorder = nullptr;
        removeTempDirectory();
        g_last_create_error = VA_RECORDER::error_allocation_failed;
        return false;
    }
    if (!_fs_get_new_rec_file_path(g_recorder->data.savePath, sizeof(g_recorder->data.savePath)))
    {
        delete g_recorder;
        g_recorder = nullptr;
        removeTempDirectory();
        g_last_create_error = VA_RECORDER::error_temp_prepare_failed;
        return false;
    }
    if (!g_recorder->acquireTrigger(std::move(trigger)))
    {
        delete g_recorder;
        g_recorder = nullptr;
        removeTempDirectory();
        g_last_create_error = VA_RECORDER::error_allocation_failed;
        return false;
    }
    if (xTaskCreate(recorderTask, "rec", 4000, g_recorder, 15, nullptr) != pdPASS)
    {
        spdlog::error("xTaskCreate recorder failed");
        g_recorder->taskCreateFailed();
        delete g_recorder;
        g_recorder = nullptr;
        removeTempDirectory();
        g_last_create_error = VA_RECORDER::error_task_create_failed;
        return false;
    }
    return true;
}

bool HAL_VAMeter::isVaRecorderExist()
{
    if (g_recorder == nullptr) return false;
    const RunningState_t state = g_recorder->snapshot().currentState;
    return state != daemon_state_finished && state != daemon_state_error;
}

bool HAL_VAMeter::isVaRecorderRecording()
{
    return g_recorder != nullptr && g_recorder->snapshot().currentState == daemon_state_recording;
}

bool HAL_VAMeter::isVaRecorderSaving()
{
    return g_recorder != nullptr && g_recorder->snapshot().currentState == daemon_state_wraping_up;
}

VA_RECORDER::Error_t HAL_VAMeter::getVaRecorderError()
{
    return g_recorder == nullptr ? g_last_create_error : g_recorder->snapshot().error;
}

bool HAL_VAMeter::destroyVaRecorder()
{
    if (g_recorder == nullptr) return true;
    if (isVaRecorderExist())
    {
        g_recorder->requestStop();
        const uint32_t started = esp_timer_get_time() / 1000;
        while (isVaRecorderExist() && (esp_timer_get_time() / 1000) - started < 5000U)
        {
            feedTheDog();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (isVaRecorderExist())
        {
            spdlog::error("recorder stop timed out");
            return false;
        }
    }
    g_last_create_error = g_recorder->snapshot().error;
    if (!g_recorder->releaseFinishedTrigger())
    {
        spdlog::error("recorder trigger release rejected before task finish");
        return false;
    }
    delete g_recorder;
    g_recorder = nullptr;
    spdlog::info("recorder destroyed");
    return true;
}
