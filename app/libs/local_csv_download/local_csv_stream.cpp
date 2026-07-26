/*
 * SPDX-License-Identifier: MIT
 */
#include "local_csv_stream.h"

namespace LOCAL_CSV_DOWNLOAD
{
    StreamResult StreamChunks(ReadChunkCallback readChunk,
                              void* readContext,
                              SendChunkCallback sendChunk,
                              void* sendContext,
                              std::uint8_t* buffer,
                              std::size_t bufferSize,
                              StreamStats* stats)
    {
        StreamStats localStats = {0, 0, 0};
        if (stats != nullptr)
            *stats = localStats;

        if (readChunk == nullptr || sendChunk == nullptr || buffer == nullptr || bufferSize == 0)
            return StreamResult::InvalidArgument;

        while (true)
        {
            bool readFailed = false;
            const std::size_t readSize = readChunk(readContext, buffer, bufferSize, &readFailed);
            if (readSize > bufferSize)
                return StreamResult::ReadFailed;

            localStats.bytesRead += readSize;

            if (readSize > 0)
            {
                if (!sendChunk(sendContext, buffer, readSize))
                {
                    if (stats != nullptr)
                        *stats = localStats;
                    return StreamResult::SendFailed;
                }

                localStats.bytesSent += readSize;
                ++localStats.chunksSent;
            }

            if (stats != nullptr)
                *stats = localStats;

            if (readFailed)
                return StreamResult::ReadFailed;

            if (readSize < bufferSize)
                return StreamResult::Complete;
        }
    }

    StreamResult FinishChunks(FinishChunkCallback finishChunk, void* finishContext)
    {
        if (finishChunk == nullptr)
            return StreamResult::InvalidArgument;

        return finishChunk(finishContext) ? StreamResult::Complete : StreamResult::FinishFailed;
    }
}
