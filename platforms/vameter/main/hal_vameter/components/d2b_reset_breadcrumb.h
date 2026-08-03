#pragma once

#include "libs/runtime_evidence/reset_breadcrumb.h"

#include <cstdint>

namespace D2B_RESET_BREADCRUMB_ADAPTER
{
    using Stage = D2B_RESET_BREADCRUMB::Stage;
    using Record = D2B_RESET_BREADCRUMB::Record;

    // Boot is called before application task registration.  BeginBoot copies
    // the previous complete RTC record; the caller emits its one compact
    // report and then calls MarkBootReported for the final 010 commit.
    void BeginBoot(std::uint32_t resetReasonCode, std::uint32_t resetReasonRaw);
    void MarkBootReported();
    void LogBoot(std::uint32_t resetReasonCode, std::uint32_t resetReasonRaw);

    // The replay is intentionally callable only from the application/UI
    // server-start path; no HTTPD callback invokes it.
    void ReplayPriorSnapshot();

    void MarkApplicationStage(Stage stage,
                               std::uint32_t serverGeneration,
                               std::uint32_t websocketGeneration = 0U,
                               std::int32_t socket = -1,
                               std::uint32_t streamId = 0U);

    void MarkHttpdStage(Stage stage,
                        std::uint32_t serverGeneration,
                        std::uint32_t websocketGeneration,
                        std::int32_t socket,
                        std::uint32_t streamId);

    bool ReadLatest(Record& record);
    bool ReadPriorSnapshot(Record& record);
    const char* StageName(std::uint32_t stage);
} // namespace D2B_RESET_BREADCRUMB_ADAPTER
