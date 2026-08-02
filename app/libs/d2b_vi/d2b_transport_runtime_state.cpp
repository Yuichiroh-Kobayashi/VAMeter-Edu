#include "d2b_transport_runtime_state.h"

namespace D2B
{
    bool IsPublicStreaming(const PublicStreamingSnapshot& snapshot)
    {
        if (!snapshot.transportOwnerActive || !snapshot.sessionStreaming || snapshot.transportStreamId == 0 ||
            snapshot.transportServerHandleKey == 0 || snapshot.transportServerGeneration == 0)
            return false;
        if (!snapshot.pipelineInitialized || !snapshot.pipelineOwnerOpen || !snapshot.pipelineStreamActive ||
            snapshot.pipelineStopping || snapshot.pipelineTaskFault || snapshot.pipelineStreamId == 0)
            return false;
        if (!snapshot.producerActive || snapshot.producerStreamId == 0)
            return false;
        if (!snapshot.lifecycleAccepting || snapshot.lifecycleServerHandleKey == 0 ||
            snapshot.lifecycleServerGeneration == 0)
            return false;

        return snapshot.transportStreamId == snapshot.pipelineStreamId &&
               snapshot.pipelineStreamId == snapshot.producerStreamId &&
               snapshot.transportServerHandleKey == snapshot.lifecycleServerHandleKey &&
               snapshot.transportServerGeneration == snapshot.lifecycleServerGeneration;
    }
} // namespace D2B
