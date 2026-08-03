#pragma once

#include <cstdint>

#include "libs/runtime_evidence/runtime_evidence.h"
#include "d2b_reset_breadcrumb.h"

namespace D2B_PIPELINE
{
    struct OwnerKey;
    struct Snapshot;
} // namespace D2B_PIPELINE

namespace D2B_PRODUCER
{
    struct Snapshot;
} // namespace D2B_PRODUCER

namespace D2B_RUNTIME_EVIDENCE
{
    using BreadcrumbStage = D2B_RESET_BREADCRUMB_ADAPTER::Stage;

    void MarkApplicationStage(BreadcrumbStage stage,
                              std::uint32_t serverGeneration,
                              std::uint32_t websocketGeneration = 0U,
                              std::int32_t socket = -1,
                              std::uint32_t streamId = 0U);
    void MarkHttpdStage(BreadcrumbStage stage,
                        std::uint32_t serverGeneration,
                        std::uint32_t websocketGeneration,
                        std::int32_t socket,
                        std::uint32_t streamId);
    void ReplayPriorBreadcrumbSnapshot();

    void SetServerGeneration(std::uint32_t generation);

    void LogServerRequest(RUNTIME_EVIDENCE::Event event,
                          RUNTIME_EVIDENCE::Owner owner,
                          RUNTIME_EVIDENCE::Reason reason,
                          std::uint32_t generation);
    void LogServerResult(RUNTIME_EVIDENCE::Event event,
                         RUNTIME_EVIDENCE::Owner owner,
                         RUNTIME_EVIDENCE::Reason reason,
                         RUNTIME_EVIDENCE::Result result,
                         std::uint32_t generation);

    void LogWebSocketConnect(const D2B_PIPELINE::OwnerKey& owner);
    void LogWebSocketDisconnect(const D2B_PIPELINE::OwnerKey& owner, RUNTIME_EVIDENCE::Reason reason);

    void LogStreamStart(const D2B_PIPELINE::OwnerKey& owner,
                        std::uint32_t streamId,
                        const D2B_PIPELINE::Snapshot& pipeline,
                        const D2B_PRODUCER::Snapshot& producer);
    void LogStreamStopAccepted(const D2B_PIPELINE::OwnerKey& owner,
                               std::uint32_t streamId,
                               const D2B_PIPELINE::Snapshot& pipeline,
                               const D2B_PRODUCER::Snapshot& producer);
    void LogStreamStopCompleted(const D2B_PIPELINE::OwnerKey& owner,
                                std::uint32_t streamId,
                                const D2B_PIPELINE::Snapshot& pipeline,
                                const D2B_PRODUCER::Snapshot& producer);
    void LogStreamAbrupt(const D2B_PIPELINE::OwnerKey& owner,
                         std::uint32_t streamId,
                         RUNTIME_EVIDENCE::Reason reason,
                         const D2B_PIPELINE::Snapshot& pipeline,
                         const D2B_PRODUCER::Snapshot& producer);
    void LogSendFailure(const D2B_PIPELINE::OwnerKey& owner,
                        std::uint32_t streamId,
                        RUNTIME_EVIDENCE::Reason reason,
                        const D2B_PIPELINE::Snapshot& pipeline,
                        const D2B_PRODUCER::Snapshot& producer);

    void LogQueueSnapshot(const D2B_PIPELINE::OwnerKey& owner,
                          const D2B_PIPELINE::Snapshot& pipeline,
                          const D2B_PRODUCER::Snapshot& producer,
                          RUNTIME_EVIDENCE::Reason reason);
    void LogActiveTrend(const D2B_PIPELINE::OwnerKey& owner,
                        const D2B_PIPELINE::Snapshot& pipeline,
                        const D2B_PRODUCER::Snapshot& producer);

    void LogPumpScheduleAccepted(const D2B_PIPELINE::OwnerKey& owner,
                                 const D2B_PIPELINE::Snapshot& pipeline,
                                 const D2B_PRODUCER::Snapshot& producer);
    void LogPumpScheduleCoalesced(const D2B_PIPELINE::OwnerKey& owner,
                                  const D2B_PIPELINE::Snapshot& pipeline,
                                  const D2B_PRODUCER::Snapshot& producer);
    void LogPumpQueueRejected(const D2B_PIPELINE::OwnerKey& owner,
                              const D2B_PIPELINE::Snapshot& pipeline,
                              const D2B_PRODUCER::Snapshot& producer);
    void LogPumpCallbackBegin(const D2B_PIPELINE::OwnerKey& owner,
                              const D2B_PIPELINE::Snapshot& pipeline,
                              const D2B_PRODUCER::Snapshot& producer);
    void LogPumpCallbackEnd(const D2B_PIPELINE::OwnerKey& owner,
                            const D2B_PIPELINE::Snapshot& pipeline,
                            const D2B_PRODUCER::Snapshot& producer);
    void LogPumpStale(const D2B_PIPELINE::OwnerKey& owner,
                      const D2B_PIPELINE::Snapshot& pipeline,
                      const D2B_PRODUCER::Snapshot& producer);

    void LogBoot();
} // namespace D2B_RUNTIME_EVIDENCE
