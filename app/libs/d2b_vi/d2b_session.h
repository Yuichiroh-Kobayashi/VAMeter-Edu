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
    void BuildErrorResponseInto(ErrorCode error, ControlResponse& output);
    void HandleClientMessageInto(Session& session,
                                 const ClientMessage& message,
                                 std::uint32_t& streamIdCounter,
                                 ControlResponse& output);

    // Compatibility wrappers for host callers. The product control hot path
    // uses the Into APIs so response storage is owned by Transport.
    ControlResponse BuildErrorResponse(ErrorCode error);
    ControlResponse HandleClientMessage(Session& session, const ClientMessage& message, std::uint32_t& streamIdCounter);
    ControlResponse BuildStatusResponse(const Session& session,
                                        std::uint64_t uptimeUs,
                                        std::uint64_t producerDropCount,
                                        std::uint64_t outputQueueDropCount,
                                        std::uint32_t queuedSampleCount);
} // namespace D2B
