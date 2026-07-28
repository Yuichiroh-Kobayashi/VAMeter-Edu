/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace RECORD_CSV
{
    constexpr std::size_t kMaxLineBytes = 256;

    enum LineKind
    {
        line_invalid = 0,
        line_empty,
        line_legacy_header,
        line_current_header,
        line_summary,
        line_sample,
    };

    struct ParsedLine
    {
        LineKind kind = line_invalid;
        float voltage = 0.0f;
        float current = 0.0f;
        std::uint32_t elapsedMs = 0;
        bool hasElapsedMs = false;
        std::uint32_t recordingDurationMs = 0;
        float capacity = 0.0f;
        float energy = 0.0f;
    };

    LineKind ParseLine(const char* line, ParsedLine& parsed);
    bool ReadLine(FILE* file, char* buffer, std::size_t bufferSize, bool& tooLong);
} // namespace RECORD_CSV
