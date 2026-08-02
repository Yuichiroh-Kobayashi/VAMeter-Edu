#pragma once

#include <esp_http_server.h>

#include <cstddef>
#include <cstdint>

#include "libs/runtime_evidence/runtime_evidence.h"

namespace D2B_PIPELINE
{
    struct OwnerKey
    {
        httpd_handle_t server;
        int socket;
        std::uint32_t generation;
    };

    struct Snapshot
    {
        bool initialized;
        bool ownerOpen;
        bool streamActive;
        bool stopping;
        bool taskFault;
        std::uint32_t streamId;
        std::uint32_t queuedOutputFrames;
        std::uint64_t outputQueueDropCount;
        std::uint32_t encoderStackHighWaterBytes;
        std::uint32_t txStackHighWaterBytes;
    };

    typedef bool (*StartPublicationCallback)(void* context);

    bool Initialize();
    bool CommitServerRunning(httpd_handle_t server, std::uint32_t generation);
    bool Open(const OwnerKey& owner);
    void Close(const OwnerKey& owner,
               RUNTIME_EVIDENCE::Reason reason = RUNTIME_EVIDENCE::Reason::Disconnect);
    void StopServer(httpd_handle_t server,
                    RUNTIME_EVIDENCE::Reason reason = RUNTIME_EVIDENCE::Reason::ServerStop);
    void QuiesceSends();
    void MarkStopFailed(httpd_handle_t server);
    void MarkStopSucceeded(httpd_handle_t server);

    bool StartStream(const OwnerKey& owner, std::uint32_t streamId);
    bool StartStream(const OwnerKey& owner,
                     std::uint32_t streamId,
                     StartPublicationCallback publish,
                     void* publishContext);
    bool RequestOrderlyStop(const OwnerKey& owner,
                            std::uint32_t streamId,
                            const char* stoppedResponse,
                            std::size_t stoppedResponseSize);
    bool StopPending(const OwnerKey& owner);
    Snapshot GetSnapshot();
} // namespace D2B_PIPELINE
