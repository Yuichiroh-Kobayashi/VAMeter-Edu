/*
 * SPDX-License-Identifier: MIT
 */
#include "recorder_finalizer.h"

#include <cstring>
#include <vector>

namespace RECORDER_FINALIZER
{
    namespace
    {
        constexpr std::size_t kFormattedRowBytes = RECORD_CSV::kMaxLineBytes;

        bool ValidSequence(const SampleSequence& sequence) { return sequence.count == 0 || sequence.samples != nullptr; }

        Result Failed(const FileOperations& operations, FailureStage stage, std::size_t rowsWritten, bool closeStaging = false)
        {
            Result result = {save_failed, stage, false, rowsWritten};
            if (closeStaging && operations.close != nullptr)
                operations.close(operations.context);
            result.cleanupSucceeded = operations.removeStaging != nullptr && operations.removeStaging(operations.context);
            return result;
        }

        bool Flush(const FileOperations& operations, std::vector<char>& batch)
        {
            if (batch.empty())
                return true;
            if (!operations.write(operations.context, &batch[0], batch.size()))
                return false;
            batch.clear();
            if (operations.feedWatchdog != nullptr)
                operations.feedWatchdog(operations.context);
            return true;
        }
    } // namespace

    Result Finalize(const FileOperations& operations,
                    RECORD_CSV::OutputMode mode,
                    const SampleSequence& orderedPretrigger,
                    const SampleSequence& captured,
                    std::size_t rowsPerBatch)
    {
        if (operations.open == nullptr || operations.write == nullptr || operations.close == nullptr ||
            operations.publish == nullptr || operations.removeStaging == nullptr || rowsPerBatch == 0 ||
            rowsPerBatch > kMaximumRowsPerBatch || !ValidSequence(orderedPretrigger) || !ValidSequence(captured))
            return Failed(operations, failure_invalid_input, 0);
        if (!operations.open(operations.context))
            return Failed(operations, failure_open, 0);

        char formatted[kFormattedRowBytes] = {0};
        std::size_t formattedSize = 0;
        if (!RECORD_CSV::FormatHeader(formatted, sizeof(formatted), formattedSize) ||
            !operations.write(operations.context, formatted, formattedSize))
            return Failed(operations, failure_header_write, 0, true);
        if (operations.feedWatchdog != nullptr)
            operations.feedWatchdog(operations.context);

        std::vector<char> batch;
        batch.reserve(rowsPerBatch * kFormattedRowBytes);
        std::size_t rowsWritten = 0;
        std::size_t rowsInBatch = 0;
        const SampleSequence sequences[] = {orderedPretrigger, captured};
        for (std::size_t sequenceIndex = 0; sequenceIndex < 2; ++sequenceIndex)
        {
            const SampleSequence& sequence = sequences[sequenceIndex];
            for (std::size_t i = 0; i < sequence.count; ++i)
            {
                const RECORDER_SAMPLE_BUFFER::RecordedSample& sample = sequence.samples[i];
                if (!RECORD_CSV::FormatSample(
                        formatted, sizeof(formatted), formattedSize, mode, sample.voltage, sample.current, sample.elapsedMs))
                    return Failed(operations, failure_row_format, rowsWritten, true);
                batch.insert(batch.end(), formatted, formatted + formattedSize);
                ++rowsInBatch;
                if (rowsInBatch == rowsPerBatch)
                {
                    if (!Flush(operations, batch))
                        return Failed(operations, failure_row_write, rowsWritten, true);
                    rowsWritten += rowsInBatch;
                    rowsInBatch = 0;
                }
            }
        }
        if (!Flush(operations, batch))
            return Failed(operations, failure_row_write, rowsWritten, true);
        rowsWritten += rowsInBatch;

        if (!operations.close(operations.context))
            return Failed(operations, failure_close, rowsWritten);
        if (!operations.publish(operations.context))
            return Failed(operations, failure_publish, rowsWritten);
        Result result = {save_published, failure_none, true, rowsWritten};
        return result;
    }
} // namespace RECORDER_FINALIZER
