/*
 * SPDX-License-Identifier: MIT
 */
#include "record_csv.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace RECORD_CSV
{
    namespace
    {
        constexpr std::size_t kMaxColumns = 16;

        char* Trim(char* token)
        {
            while (*token == ' ' || *token == '\t')
                ++token;
            char* end = token + std::strlen(token);
            while (end != token && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
                --end;
            *end = '\0';
            return token;
        }

        bool ParseFloat(const char* token, float& value)
        {
            if (token == nullptr || *token == '\0')
                return false;
            errno = 0;
            char* end = nullptr;
            const float parsed = std::strtof(token, &end);
            if (errno == ERANGE || end == token || *end != '\0' || !std::isfinite(parsed))
                return false;
            value = parsed;
            return true;
        }

        bool ParseUint32(const char* token, std::uint32_t& value)
        {
            if (token == nullptr || *token == '\0' || *token == '-')
                return false;
            errno = 0;
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(token, &end, 10);
            if (errno == ERANGE || end == token || *end != '\0' || parsed > std::numeric_limits<std::uint32_t>::max())
                return false;
            value = static_cast<std::uint32_t>(parsed);
            return true;
        }

        std::size_t SplitColumns(char* line, char** columns, std::size_t capacity)
        {
            std::size_t count = 0;
            char* current = line;
            while (count < capacity)
            {
                char* comma = std::strchr(current, ',');
                if (comma != nullptr)
                    *comma = '\0';
                columns[count++] = Trim(current);
                if (comma == nullptr)
                    break;
                current = comma + 1;
            }
            return count;
        }
    } // namespace

    LineKind ParseLine(const char* line, ParsedLine& parsed)
    {
        parsed = ParsedLine();
        if (line == nullptr)
            return parsed.kind;

        const std::size_t length = std::strlen(line);
        if (length >= kMaxLineBytes)
            return parsed.kind;

        char copy[kMaxLineBytes] = {0};
        std::memcpy(copy, line, length + 1);
        char* columns[kMaxColumns] = {nullptr};
        const std::size_t count = SplitColumns(copy, columns, kMaxColumns);
        if (count == 0 || (count == 1 && columns[0][0] == '\0'))
        {
            parsed.kind = line_empty;
            return parsed.kind;
        }

        if (count >= 3 && std::strcmp(columns[0], "voltage") == 0 && std::strcmp(columns[1], "current") == 0)
        {
            if (std::strcmp(columns[2], "time") == 0)
            {
                if (count >= 5 && std::strcmp(columns[3], "capacity") == 0 && std::strcmp(columns[4], "energy") == 0)
                    parsed.kind = line_legacy_header;
            }
            else if (std::strcmp(columns[2], "elapsed_ms") == 0)
            {
                if (count == 3 ||
                    (count >= 5 && std::strcmp(columns[3], "capacity") == 0 && std::strcmp(columns[4], "energy") == 0))
                    parsed.kind = line_current_header;
            }
            return parsed.kind;
        }

        if (count >= 5 && columns[0][0] == '\0' && columns[1][0] == '\0')
        {
            if (!ParseUint32(columns[2], parsed.recordingDurationMs) || !ParseFloat(columns[3], parsed.capacity) ||
                !ParseFloat(columns[4], parsed.energy))
                return parsed.kind;
            parsed.kind = line_summary;
            return parsed.kind;
        }

        if (count < 2)
            return parsed.kind;

        parsed.hasVoltage = columns[0][0] != '\0';
        parsed.hasCurrent = columns[1][0] != '\0';
        if ((parsed.hasVoltage && !ParseFloat(columns[0], parsed.voltage)) ||
            (parsed.hasCurrent && !ParseFloat(columns[1], parsed.current)))
            return parsed.kind;
        if (count == 2)
        {
            if (!parsed.hasVoltage || !parsed.hasCurrent)
                return parsed.kind;
        }
        else
        {
            if (!ParseUint32(columns[2], parsed.elapsedMs))
                return parsed.kind;
            if (!parsed.hasVoltage && !parsed.hasCurrent)
                return parsed.kind;
            parsed.hasElapsedMs = true;
        }
        parsed.kind = line_sample;
        return parsed.kind;
    }

    bool ReadLine(FILE* file, char* buffer, std::size_t bufferSize, bool& tooLong)
    {
        tooLong = false;
        if (file == nullptr || buffer == nullptr || bufferSize < 2 || std::fgets(buffer, static_cast<int>(bufferSize), file) == nullptr)
            return false;

        const std::size_t length = std::strlen(buffer);
        if (length != 0 && buffer[length - 1] == '\n')
            return true;
        if (std::feof(file))
            return true;

        tooLong = true;
        int value = 0;
        while ((value = std::fgetc(file)) != '\n' && value != EOF)
        {
        }
        return true;
    }

    const char* Header() { return "voltage,current,elapsed_ms\n"; }

    bool WriteHeader(FILE* file) { return file != nullptr && std::fputs(Header(), file) >= 0; }

    bool WriteSample(FILE* file, OutputMode mode, float voltage, float current, std::uint32_t elapsedMs)
    {
        if (file == nullptr)
            return false;
        int result = -1;
        if (mode == output_voltage)
            result = std::fprintf(file, "%.4f,,%lu\n", voltage, static_cast<unsigned long>(elapsedMs));
        else if (mode == output_current)
            result = std::fprintf(file, ",%.7f,%lu\n", current, static_cast<unsigned long>(elapsedMs));
        else
            result = std::fprintf(
                file, "%.4f,%.7f,%lu\n", voltage, current, static_cast<unsigned long>(elapsedMs));
        return result >= 0;
    }
} // namespace RECORD_CSV
