#pragma once

#include "d2b_control.h"

#include <cstddef>
#include <cstdint>

namespace D2B
{
    static const std::size_t kMaximumControlResponseSize = 1152;

    struct ControlResponse
    {
        ErrorCode error;
        std::size_t size;
        char data[kMaximumControlResponseSize];

        bool ok() const { return error == ErrorCode::None; }
    };

    struct Session
    {
        ControlState state;
        bool ownsStream;
        std::uint32_t streamId;
    };

    void OpenSession(Session& session);
    void CloseSession(Session& session);
    ControlResponse BuildErrorResponse(ErrorCode error);
    ControlResponse HandleClientMessage(Session& session, const ClientMessage& message, std::uint32_t& streamIdCounter);
    ControlResponse BuildStatusResponse(const Session& session,
                                        std::uint64_t uptimeUs,
                                        std::uint64_t producerDropCount,
                                        std::uint64_t outputQueueDropCount,
                                        std::uint32_t queuedSampleCount);
} // namespace D2B
