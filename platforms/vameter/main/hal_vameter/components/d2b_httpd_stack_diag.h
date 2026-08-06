#pragma once

#include "libs/httpd_stack_diag/httpd_stack_diag.h"

#include <cstddef>
#include <cstdint>

namespace D2B_HTTPD_STACK_DIAG
{
    using Stage = HTTPD_STACK_DIAG::Stage;

    void BootInitialize(std::uint32_t resetReasonNormalized, std::uint32_t resetReasonRaw);
    void LogBootPrior();
    void SetConfiguredStackBytes(std::uint32_t actualStackBytes);
    void LogConfiguredStack();

    // This is the only hot-path entry point.  It samples and stores fixed-width
    // numeric state only; it does not allocate, format, log, block, or recurse.
    void Capture(Stage stage, std::uint32_t generation, std::uint32_t streamId);

    // Formatting is deliberately separate from Capture.  Call this only at a
    // shallow HTTPD boundary after the protocol response/cleanup is complete.
    void EmitSnapshot();

    std::size_t StaticStateBytes();
    std::size_t RtcStateBytes();
} // namespace D2B_HTTPD_STACK_DIAG
