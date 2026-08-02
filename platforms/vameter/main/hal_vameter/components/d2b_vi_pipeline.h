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

    bool Initialize();
    bool Open(const OwnerKey& owner);
    void Close(const OwnerKey& owner,
               RUNTIME_EVIDENCE::Reason reason = RUNTIME_EVIDENCE::Reason::Disconnect);
    void StopServer(httpd_handle_t server,
                    RUNTIME_EVIDENCE::Reason reason = RUNTIME_EVIDENCE::Reason::ServerStop);
    void QuiesceSends();

    esp_err_t SendText(const OwnerKey& owner, const char* payload, std::size_t size);
    bool StartStream(const OwnerKey& owner, std::uint32_t streamId);
    bool RequestOrderlyStop(const OwnerKey& owner,
                            std::uint32_t streamId,
                            const char* stoppedResponse,
                            std::size_t stoppedResponseSize);
    bool StopPending(const OwnerKey& owner);
    Snapshot GetSnapshot();
} // namespace D2B_PIPELINE
