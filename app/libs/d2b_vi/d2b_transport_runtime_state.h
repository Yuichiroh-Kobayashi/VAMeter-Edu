#pragma once

#include <cstdint>

namespace D2B
{
    // One allocation-free snapshot used by the public status endpoint.  The
    // fields are copied from each owner while its own lock is held, then
    // evaluated without observing a partially updated stream.
    struct PublicStreamingSnapshot
    {
        bool transportOwnerActive;
        bool sessionStreaming;
        std::uint32_t transportStreamId;
        std::uintptr_t transportServerHandleKey;
        std::uint32_t transportServerGeneration;

        bool pipelineInitialized;
        bool pipelineOwnerOpen;
        bool pipelineStreamActive;
        bool pipelineStopping;
        bool pipelineTaskFault;
        std::uint32_t pipelineStreamId;

        bool producerActive;
        std::uint32_t producerStreamId;

        bool lifecycleAccepting;
        std::uintptr_t lifecycleServerHandleKey;
        std::uint32_t lifecycleServerGeneration;
    };

    bool IsPublicStreaming(const PublicStreamingSnapshot& snapshot);
} // namespace D2B
