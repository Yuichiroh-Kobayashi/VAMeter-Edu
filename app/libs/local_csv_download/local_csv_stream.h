/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace LOCAL_CSV_DOWNLOAD
{
    const std::size_t kStreamBufferSize = 8U * 1024U;

    enum class StreamResult
    {
        Complete,
        InvalidArgument,
        ReadFailed,
        SendFailed,
        FinishFailed,
    };

    struct StreamStats
    {
        std::size_t bytesRead;
        std::size_t bytesSent;
        std::size_t chunksSent;
    };

    // A reader returns at most bufferSize bytes and sets readFailed when the
    // underlying source reports an error. A short read without an error is EOF.
    typedef std::size_t (*ReadChunkCallback)(void* context,
                                             std::uint8_t* buffer,
                                             std::size_t bufferSize,
                                             bool* readFailed);
    typedef bool (*SendChunkCallback)(void* context, const std::uint8_t* data, std::size_t dataSize);
    typedef bool (*FinishChunkCallback)(void* context);

    StreamResult StreamChunks(ReadChunkCallback readChunk,
                              void* readContext,
                              SendChunkCallback sendChunk,
                              void* sendContext,
                              std::uint8_t* buffer,
                              std::size_t bufferSize,
                              StreamStats* stats);

    StreamResult FinishChunks(FinishChunkCallback finishChunk, void* finishContext);
}
