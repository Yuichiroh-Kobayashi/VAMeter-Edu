/*
 * SPDX-License-Identifier: MIT
 */
#include "recorder_finalizer.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int failures = 0;
    void Check(bool condition, const char* expression, int line)
    {
        if (!condition)
        {
            std::cerr << "line " << line << ": CHECK failed: " << expression << '\n';
            ++failures;
        }
    }
#define CHECK(expression) Check((expression), #expression, __LINE__)

    struct FakeFile
    {
        bool openResult = true;
        bool closeResult = true;
        bool publishResult = true;
        bool cleanupResult = true;
        std::size_t failWrite = 0;
        std::size_t writeCalls = 0;
        std::size_t openCalls = 0;
        std::size_t closeCalls = 0;
        std::size_t publishCalls = 0;
        std::size_t cleanupCalls = 0;
        std::size_t watchdogCalls = 0;
        std::string bytes;
        std::vector<std::string> events;
        std::vector<std::size_t> writeSizes;
    };

    bool Open(void* context)
    {
        FakeFile& file = *static_cast<FakeFile*>(context);
        ++file.openCalls;
        file.events.push_back("open");
        return file.openResult;
    }
    bool Write(void* context, const char* data, std::size_t size)
    {
        FakeFile& file = *static_cast<FakeFile*>(context);
        ++file.writeCalls;
        file.events.push_back("write");
        file.writeSizes.push_back(size);
        if (file.failWrite == file.writeCalls)
            return false;
        file.bytes.append(data, size);
        return true;
    }
    bool Close(void* context)
    {
        FakeFile& file = *static_cast<FakeFile*>(context);
        ++file.closeCalls;
        file.events.push_back("close");
        return file.closeResult;
    }
    bool Publish(void* context)
    {
        FakeFile& file = *static_cast<FakeFile*>(context);
        ++file.publishCalls;
        file.events.push_back("publish");
        return file.publishResult;
    }
    bool Cleanup(void* context)
    {
        FakeFile& file = *static_cast<FakeFile*>(context);
        ++file.cleanupCalls;
        file.events.push_back("cleanup");
        return file.cleanupResult;
    }
    void Feed(void* context)
    {
        FakeFile& file = *static_cast<FakeFile*>(context);
        ++file.watchdogCalls;
        file.events.push_back("feed");
    }

    RECORDER_FINALIZER::FileOperations Operations(FakeFile& file)
    {
        RECORDER_FINALIZER::FileOperations operations = {&file, Open, Write, Close, Publish, Cleanup, Feed};
        return operations;
    }

    RECORDER_FINALIZER::Result Run(FakeFile& file,
                                   RECORD_CSV::OutputMode mode = RECORD_CSV::output_both,
                                   std::size_t batchBufferSize = RECORDER_FINALIZER::kMaximumBatchBytes)
    {
        using RECORDER_SAMPLE_BUFFER::RecordedSample;
        static const RecordedSample captured[] = {
            {1.25f, -0.0004275f, 0},
            {-2.0f, 3.5f, 40},
            {9.0f, -7.0f, 81},
        };
        const RECORDER_FINALIZER::SampleSequence none = {nullptr, 0};
        const RECORDER_FINALIZER::SampleSequence samples = {captured, 3};
        char batchBuffer[RECORDER_FINALIZER::kMaximumBatchBytes] = {0};
        return RECORDER_FINALIZER::Finalize(Operations(file), mode, none, samples, batchBuffer, batchBufferSize);
    }

    std::string BytesFromPublicWriter(RECORD_CSV::OutputMode mode)
    {
        FILE* file = std::tmpfile();
        if (file == nullptr)
            return std::string();
        RECORD_CSV::WriteHeader(file);
        RECORD_CSV::WriteSample(file, mode, 1.25f, -0.0004275f, 0);
        RECORD_CSV::WriteSample(file, mode, -2.0f, 3.5f, 40);
        RECORD_CSV::WriteSample(file, mode, 9.0f, -7.0f, 81);
        std::rewind(file);
        std::string bytes;
        char buffer[64];
        std::size_t read = 0;
        while ((read = std::fread(buffer, 1, sizeof(buffer), file)) != 0)
            bytes.append(buffer, read);
        std::fclose(file);
        return bytes;
    }

    void TestFormattingAuthority()
    {
        char buffer[128] = {0};
        std::size_t size = 99;
        CHECK(RECORD_CSV::FormatHeader(buffer, sizeof(buffer), size));
        CHECK(std::string(buffer, size) == "voltage,current,elapsed_ms\n");
        CHECK(!RECORD_CSV::FormatHeader(buffer, size, size));
        CHECK(size == 0);

        CHECK(RECORD_CSV::FormatSample(buffer, sizeof(buffer), size, RECORD_CSV::output_voltage, -1.25f, 8.0f, 0));
        CHECK(std::string(buffer, size) == "-1.2500,,0\n");
        CHECK(
            RECORD_CSV::FormatSample(buffer, sizeof(buffer), size, RECORD_CSV::output_current, 8.0f, -0.0004275f, 4294967295U));
        CHECK(std::string(buffer, size) == ",-0.0004275,4294967295\n");
        CHECK(RECORD_CSV::FormatSample(buffer, sizeof(buffer), size, RECORD_CSV::output_both, -12.5f, 3.25f, 41));
        CHECK(std::string(buffer, size) == "-12.5000,3.2500000,41\n");
        CHECK(!RECORD_CSV::FormatSample(buffer, 4, size, RECORD_CSV::output_both, 1.0f, 2.0f, 3));
        CHECK(size == 0);
    }

    void TestSuccessModesBatchingAndWatchdog()
    {
        const RECORD_CSV::OutputMode modes[] = {
            RECORD_CSV::output_voltage, RECORD_CSV::output_current, RECORD_CSV::output_both};
        for (std::size_t i = 0; i < 3; ++i)
        {
            FakeFile file;
            const RECORDER_FINALIZER::Result result = Run(file, modes[i]);
            CHECK(result.state == RECORDER_FINALIZER::save_published);
            CHECK(result.failureStage == RECORDER_FINALIZER::failure_none);
            CHECK(result.cleanupSucceeded);
            CHECK(result.rowsWritten == 3);
            CHECK(file.bytes == BytesFromPublicWriter(modes[i]));
            CHECK(file.writeCalls == 2); // Header, then one byte-bounded sample batch.
            CHECK(file.watchdogCalls == file.writeCalls);
            for (std::size_t writeIndex = 0; writeIndex < file.writeSizes.size(); ++writeIndex)
                CHECK(file.writeSizes[writeIndex] <= RECORDER_FINALIZER::kMaximumBatchBytes);
            CHECK(file.closeCalls == 1);
            CHECK(file.publishCalls == 1);
            CHECK(file.cleanupCalls == 0);
            CHECK(file.events[file.events.size() - 2] == "close");
            CHECK(file.events.back() == "publish");
        }
    }

    void TestFailuresNoRetry()
    {
        FakeFile open;
        open.openResult = false;
        RECORDER_FINALIZER::Result result = Run(open);
        CHECK(result.failureStage == RECORDER_FINALIZER::failure_open);
        CHECK(open.openCalls == 1 && open.writeCalls == 0 && open.cleanupCalls == 1);

        FakeFile header;
        header.failWrite = 1;
        result = Run(header);
        CHECK(result.failureStage == RECORDER_FINALIZER::failure_header_write);
        CHECK(header.writeCalls == 1 && header.closeCalls == 1 && header.publishCalls == 0 && header.cleanupCalls == 1);

        FakeFile row;
        row.failWrite = 3; // Header and the first individually batched row succeed.
        result = Run(row, RECORD_CSV::output_both, 24);
        CHECK(result.failureStage == RECORDER_FINALIZER::failure_row_write);
        CHECK(result.rowsWritten == 1);
        CHECK(row.writeCalls == 3 && row.closeCalls == 1 && row.publishCalls == 0 && row.cleanupCalls == 1);

        FakeFile rowTooLarge;
        result = Run(rowTooLarge, RECORD_CSV::output_both, 4);
        CHECK(result.failureStage == RECORDER_FINALIZER::failure_row_format);
        CHECK(result.rowsWritten == 0);
        CHECK(rowTooLarge.writeCalls == 1 && rowTooLarge.closeCalls == 1 && rowTooLarge.publishCalls == 0 &&
              rowTooLarge.cleanupCalls == 1);

        FakeFile close;
        close.closeResult = false;
        result = Run(close);
        CHECK(result.failureStage == RECORDER_FINALIZER::failure_close);
        CHECK(close.closeCalls == 1 && close.publishCalls == 0 && close.cleanupCalls == 1);

        FakeFile publish;
        publish.publishResult = false;
        result = Run(publish);
        CHECK(result.failureStage == RECORDER_FINALIZER::failure_publish);
        CHECK(publish.publishCalls == 1 && publish.cleanupCalls == 1);
        CHECK(result.cleanupSucceeded);

        FakeFile cleanup;
        cleanup.publishResult = false;
        cleanup.cleanupResult = false;
        result = Run(cleanup);
        CHECK(result.failureStage == RECORDER_FINALIZER::failure_publish);
        CHECK(!result.cleanupSucceeded);
        CHECK(cleanup.publishCalls == 1 && cleanup.cleanupCalls == 1);
    }

    void TestOrderedPretriggerThenCapture()
    {
        using RECORDER_SAMPLE_BUFFER::RecordedSample;
        const RecordedSample pretrigger[] = {{0.1f, -0.1f, 0}, {0.2f, -0.2f, 0}};
        const RecordedSample captured[] = {{1.0f, 2.0f, 0}, {3.0f, 4.0f, 40}};
        const RECORDER_FINALIZER::SampleSequence before = {pretrigger, 2};
        const RECORDER_FINALIZER::SampleSequence after = {captured, 2};
        FakeFile file;
        char batchBuffer[60] = {0};
        const RECORDER_FINALIZER::Result result = RECORDER_FINALIZER::Finalize(
            Operations(file), RECORD_CSV::output_both, before, after, batchBuffer, sizeof(batchBuffer));
        CHECK(result.state == RECORDER_FINALIZER::save_published);
        CHECK(result.rowsWritten == 4);
        CHECK(file.bytes == "voltage,current,elapsed_ms\n"
                            "0.1000,-0.1000000,0\n"
                            "0.2000,-0.2000000,0\n"
                            "1.0000,2.0000000,0\n"
                            "3.0000,4.0000000,40\n");
        CHECK(file.writeCalls == 3); // Header plus two byte-bounded batches.
    }
} // namespace

int main()
{
    TestFormattingAuthority();
    TestSuccessModesBatchingAndWatchdog();
    TestFailuresNoRetry();
    TestOrderedPretriggerThenCapture();
    if (failures != 0)
        return 1;
    std::cout << "recorder finalizer tests passed\n";
    return 0;
}
