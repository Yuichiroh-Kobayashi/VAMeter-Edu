/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "record_csv.h"
#include "recorder_sample_buffer.h"

#include <cstddef>

namespace RECORDER_FINALIZER
{
    constexpr std::size_t kMaximumBatchBytes = 512;

    enum SaveState
    {
        save_published = 0,
        save_failed,
    };

    enum FailureStage
    {
        failure_none = 0,
        failure_invalid_input,
        failure_open,
        failure_header_write,
        failure_row_format,
        failure_row_write,
        failure_close,
        failure_publish,
    };

    struct Result
    {
        SaveState state;
        FailureStage failureStage;
        bool cleanupSucceeded;
        std::size_t rowsWritten;
    };

    struct FileOperations
    {
        void* context;
        bool (*open)(void* context);
        bool (*write)(void* context, const char* data, std::size_t size);
        bool (*close)(void* context);
        bool (*publish)(void* context);
        bool (*removeStaging)(void* context);
        void (*feedWatchdog)(void* context);
    };

    struct SampleSequence
    {
        const RECORDER_SAMPLE_BUFFER::RecordedSample* samples;
        std::size_t count;
    };

    // Publication means only that close and the injected runtime publish
    // operation returned success. It does not claim journaling, crash atomicity,
    // power-loss durability, or any particular filesystem rename behavior.
    Result Finalize(const FileOperations& operations,
                    RECORD_CSV::OutputMode mode,
                    const SampleSequence& orderedPretrigger,
                    const SampleSequence& captured,
                    char* batchBuffer,
                    std::size_t batchBufferSize);
} // namespace RECORDER_FINALIZER
