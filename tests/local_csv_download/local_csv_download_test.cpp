/*
 * SPDX-License-Identifier: MIT
 */
#include "local_csv_download_name.h"
#include "local_csv_download_selection.h"
#include "local_csv_stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void Check(bool condition, const char* expression, int line)
    {
        if (condition)
            return;

        std::cerr << "line " << line << ": CHECK failed: " << expression << '\n';
        ++failures;
    }

#define CHECK(expression) Check((expression), #expression, __LINE__)

    struct Reader
    {
        std::vector<std::uint8_t> data;
        std::size_t offset;
        std::size_t failAt;
    };

    std::size_t ReadChunk(void* context, std::uint8_t* buffer, std::size_t bufferSize, bool* readFailed)
    {
        Reader* reader = static_cast<Reader*>(context);
        if (reader->offset >= reader->failAt)
        {
            *readFailed = true;
            return 0;
        }

        const std::size_t available = reader->data.size() - reader->offset;
        const std::size_t beforeFailure = reader->failAt - reader->offset;
        const std::size_t readSize = std::min(bufferSize, std::min(available, beforeFailure));
        std::copy(reader->data.begin() + reader->offset,
                  reader->data.begin() + reader->offset + readSize,
                  buffer);
        reader->offset += readSize;
        *readFailed = reader->offset >= reader->failAt;
        return readSize;
    }

    struct Sender
    {
        std::vector<std::uint8_t> data;
        std::vector<std::size_t> chunkSizes;
        std::size_t failAtChunk;
    };

    bool SendChunk(void* context, const std::uint8_t* data, std::size_t dataSize)
    {
        Sender* sender = static_cast<Sender*>(context);
        if (sender->chunkSizes.size() >= sender->failAtChunk)
            return false;

        sender->data.insert(sender->data.end(), data, data + dataSize);
        sender->chunkSizes.push_back(dataSize);
        return true;
    }

    Reader MakeReader(std::size_t size)
    {
        Reader reader;
        reader.offset = 0;
        reader.failAt = static_cast<std::size_t>(-1);
        for (std::size_t i = 0; i < size; ++i)
            reader.data.push_back(static_cast<std::uint8_t>(i % 251));
        return reader;
    }

    Sender MakeSender()
    {
        Sender sender;
        sender.failAtChunk = static_cast<std::size_t>(-1);
        return sender;
    }

    LOCAL_CSV_DOWNLOAD::StreamResult RunStream(Reader& reader,
                                                Sender& sender,
                                                std::size_t bufferSize,
                                                LOCAL_CSV_DOWNLOAD::StreamStats& stats)
    {
        std::vector<std::uint8_t> buffer(bufferSize);
        return LOCAL_CSV_DOWNLOAD::StreamChunks(
            ReadChunk, &reader, SendChunk, &sender, buffer.data(), buffer.size(), &stats);
    }

    void TestRecordNames()
    {
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-0.csv"));
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-000.csv"));
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-1000.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(""));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("../REC-000.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-/000.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-\\000.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-000.txt"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-001.CSV"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-001.csv/"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("OTHER-000.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-00\n0.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-00?0.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-１２３.csv"));

        std::string nulName = "REC-00";
        nulName.push_back('\0');
        nulName += ".csv";
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(nulName));

        std::string delName = "REC-00";
        delName.push_back(static_cast<char>(0x7f));
        delName += ".csv";
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(delName));

        const std::size_t digitCountAtLimit = LOCAL_CSV_DOWNLOAD::kMaxRecordNameLength - 8;
        const std::string nameAtLimit = "REC-" + std::string(digitCountAtLimit, '0') + ".csv";
        const std::string nameOverLimit = "REC-" + std::string(digitCountAtLimit + 1, '0') + ".csv";
        CHECK(nameAtLimit.size() == LOCAL_CSV_DOWNLOAD::kMaxRecordNameLength);
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(nameAtLimit));
        CHECK(nameOverLimit.size() == LOCAL_CSV_DOWNLOAD::kMaxRecordNameLength + 1);
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(nameOverLimit));
    }

    void TestUrlDecodeOnce()
    {
        std::string decoded;
        CHECK(LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce("REC-%30%30%31.csv", decoded));
        CHECK(decoded == "REC-001.csv");
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(decoded));

        CHECK(LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce("REC-%2530.csv", decoded));
        CHECK(decoded == "REC-%30.csv");
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(decoded));

        CHECK(LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce("REC-%00.csv", decoded));
        CHECK(decoded.size() == 9);
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(decoded));

        CHECK(LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce("REC-%2F000.csv", decoded));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(decoded));
        CHECK(LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce("REC-%5C000.csv", decoded));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(decoded));
        CHECK(LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce("REC-%2E%2E.csv", decoded));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(decoded));

        CHECK(!LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce("REC-%0.csv", decoded));
        CHECK(!LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce("REC-%GG.csv", decoded));

        const std::size_t digitCountAtLimit = LOCAL_CSV_DOWNLOAD::kMaxRecordNameLength - 8;
        const std::string encodedAtLimit = "REC-" + std::string(digitCountAtLimit, '1') + ".csv";
        CHECK(LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce(encodedAtLimit, decoded));
        CHECK(decoded.size() == LOCAL_CSV_DOWNLOAD::kMaxRecordNameLength);
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(decoded));

        const std::string encodedOverLimit = "REC-" + std::string(digitCountAtLimit + 1, '1') + ".csv";
        CHECK(!LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce(encodedOverLimit, decoded));
    }

    void TestEmptyFile()
    {
        Reader reader = MakeReader(0);
        Sender sender = MakeSender();
        LOCAL_CSV_DOWNLOAD::StreamStats stats;

        CHECK(RunStream(reader, sender, 8, stats) == LOCAL_CSV_DOWNLOAD::StreamResult::Complete);
        CHECK(sender.data.empty());
        CHECK(sender.chunkSizes.empty());
        CHECK(stats.bytesRead == 0);
        CHECK(stats.bytesSent == 0);
    }

    void TestMultipleChunksAndShortFinalChunk()
    {
        Reader reader = MakeReader(21);
        const std::vector<std::uint8_t> expected = reader.data;
        Sender sender = MakeSender();
        LOCAL_CSV_DOWNLOAD::StreamStats stats;

        CHECK(RunStream(reader, sender, 8, stats) == LOCAL_CSV_DOWNLOAD::StreamResult::Complete);
        CHECK(sender.data == expected);
        CHECK(sender.chunkSizes.size() == 3);
        CHECK(sender.chunkSizes[0] == 8);
        CHECK(sender.chunkSizes[1] == 8);
        CHECK(sender.chunkSizes[2] == 5);
        CHECK(stats.bytesRead == 21);
        CHECK(stats.bytesSent == 21);
        CHECK(stats.chunksSent == 3);
    }

    void TestExactMultipleEndsAtEof()
    {
        Reader reader = MakeReader(16);
        Sender sender = MakeSender();
        LOCAL_CSV_DOWNLOAD::StreamStats stats;

        CHECK(RunStream(reader, sender, 8, stats) == LOCAL_CSV_DOWNLOAD::StreamResult::Complete);
        CHECK(sender.chunkSizes.size() == 2);
        CHECK(stats.bytesRead == 16);
        CHECK(stats.bytesSent == 16);
    }

    void TestReadFailure()
    {
        Reader reader = MakeReader(30);
        reader.failAt = 10;
        Sender sender = MakeSender();
        LOCAL_CSV_DOWNLOAD::StreamStats stats;

        CHECK(RunStream(reader, sender, 8, stats) == LOCAL_CSV_DOWNLOAD::StreamResult::ReadFailed);
        CHECK(stats.bytesRead == 10);
        CHECK(stats.bytesSent == 10);
        CHECK(sender.chunkSizes.size() == 2);
        CHECK(sender.chunkSizes[1] == 2);
    }

    void TestZeroByteReadFailure()
    {
        Reader reader = MakeReader(0);
        reader.failAt = 0;
        Sender sender = MakeSender();
        LOCAL_CSV_DOWNLOAD::StreamStats stats;

        CHECK(RunStream(reader, sender, 8, stats) == LOCAL_CSV_DOWNLOAD::StreamResult::ReadFailed);
        CHECK(sender.chunkSizes.empty());
        CHECK(stats.bytesRead == 0);
        CHECK(stats.bytesSent == 0);
    }

    bool FinishChunks(void* context)
    {
        return *static_cast<bool*>(context);
    }

    void TestFinishFailure()
    {
        bool finishSucceeds = true;
        CHECK(LOCAL_CSV_DOWNLOAD::FinishChunks(FinishChunks, &finishSucceeds) ==
              LOCAL_CSV_DOWNLOAD::StreamResult::Complete);

        finishSucceeds = false;
        CHECK(LOCAL_CSV_DOWNLOAD::FinishChunks(FinishChunks, &finishSucceeds) ==
              LOCAL_CSV_DOWNLOAD::StreamResult::FinishFailed);
    }

    void TestSendFailure()
    {
        Reader reader = MakeReader(30);
        Sender sender = MakeSender();
        sender.failAtChunk = 1;
        LOCAL_CSV_DOWNLOAD::StreamStats stats;

        CHECK(RunStream(reader, sender, 8, stats) == LOCAL_CSV_DOWNLOAD::StreamResult::SendFailed);
        CHECK(stats.bytesRead == 16);
        CHECK(stats.bytesSent == 8);
        CHECK(stats.chunksSent == 1);
        CHECK(sender.data.size() == 8);
    }

    void TestFirstChunkSendFailure()
    {
        Reader reader = MakeReader(30);
        Sender sender = MakeSender();
        sender.failAtChunk = 0;
        LOCAL_CSV_DOWNLOAD::StreamStats stats;

        CHECK(RunStream(reader, sender, 8, stats) == LOCAL_CSV_DOWNLOAD::StreamResult::SendFailed);
        CHECK(stats.bytesRead == 8);
        CHECK(stats.bytesSent == 0);
        CHECK(stats.chunksSent == 0);
        CHECK(sender.data.empty());
    }

    void TestInvalidStreamArguments()
    {
        Reader reader = MakeReader(8);
        Sender sender = MakeSender();
        std::uint8_t buffer[8];

        CHECK(LOCAL_CSV_DOWNLOAD::StreamChunks(
                  nullptr, &reader, SendChunk, &sender, buffer, sizeof(buffer), nullptr) ==
              LOCAL_CSV_DOWNLOAD::StreamResult::InvalidArgument);
        CHECK(LOCAL_CSV_DOWNLOAD::StreamChunks(
                  ReadChunk, &reader, nullptr, &sender, buffer, sizeof(buffer), nullptr) ==
              LOCAL_CSV_DOWNLOAD::StreamResult::InvalidArgument);
        CHECK(LOCAL_CSV_DOWNLOAD::StreamChunks(
                  ReadChunk, &reader, SendChunk, &sender, nullptr, sizeof(buffer), nullptr) ==
              LOCAL_CSV_DOWNLOAD::StreamResult::InvalidArgument);
        CHECK(LOCAL_CSV_DOWNLOAD::StreamChunks(
                  ReadChunk, &reader, SendChunk, &sender, buffer, 0, nullptr) ==
              LOCAL_CSV_DOWNLOAD::StreamResult::InvalidArgument);
        CHECK(LOCAL_CSV_DOWNLOAD::FinishChunks(nullptr, nullptr) ==
              LOCAL_CSV_DOWNLOAD::StreamResult::InvalidArgument);
    }

    void TestDownloadSelection()
    {
        LOCAL_CSV_DOWNLOAD::DownloadSelection selection;

        LOCAL_CSV_DOWNLOAD::DownloadSelectionSnapshot current = selection.snapshot();
        CHECK(current.name.empty());
        CHECK(current.path.empty());

        selection.set("REC-1.csv", "/data/REC-1.csv");
        current = selection.snapshot();
        CHECK(current.name == "REC-1.csv");
        CHECK(current.path == "/data/REC-1.csv");

        selection.set("REC-2.csv", "/data/REC-2.csv");
        CHECK(current.name == "REC-1.csv");
        CHECK(current.path == "/data/REC-1.csv");

        current = selection.snapshot();
        CHECK(current.name == "REC-2.csv");
        CHECK(current.path == "/data/REC-2.csv");

        selection.clear();
        current = selection.snapshot();
        CHECK(current.name.empty());
        CHECK(current.path.empty());
    }
}

int main()
{
    TestRecordNames();
    TestUrlDecodeOnce();
    TestEmptyFile();
    TestMultipleChunksAndShortFinalChunk();
    TestExactMultipleEndsAtEof();
    TestReadFailure();
    TestZeroByteReadFailure();
    TestSendFailure();
    TestFirstChunkSendFailure();
    TestFinishFailure();
    TestInvalidStreamArguments();
    TestDownloadSelection();

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "local CSV download tests passed\n";
    return 0;
}
