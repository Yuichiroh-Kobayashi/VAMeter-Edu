/*
 * SPDX-License-Identifier: MIT
 */
#include "motor_observe_csv_logger.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

namespace MOTOR_OBSERVE
{
    namespace CSV
    {
        namespace
        {
            const char* kCsvHeader =
                "schema_version,session_id,sample_index,t_ms,t_s,event,safety_state,physical_output_allowed,"
                "requested_target_percent,applied_target_percent,output_pattern,gpio9_duty_percent,"
                "gpio8_duty_percent,measurement_path_code,voltage_semantics,current_semantics,power_semantics,"
                "voltage_v,current_a,power_w,power_source,pwm_waveform_captured,abnormal_flag,note";

            std::string escapeCsvField(const std::string& input)
            {
                const bool needsQuoting = input.find_first_of(",\"\r\n") != std::string::npos;
                if (!needsQuoting)
                    return input;

                std::string escaped = "\"";
                for (char ch : input)
                {
                    if (ch == '"')
                        escaped += "\"\"";
                    else
                        escaped += ch;
                }
                escaped += '"';
                return escaped;
            }

            bool hasMoCsvName(const char* fileName)
            {
                if (fileName == nullptr)
                    return false;

                const size_t length = std::strlen(fileName);
                if (length != std::strlen("MO-000.csv"))
                    return false;

                if (std::strncmp(fileName, "MO-", 3) != 0)
                    return false;

                if (std::strcmp(fileName + length - 4, ".csv") != 0)
                    return false;

                return std::isdigit(static_cast<unsigned char>(fileName[3])) &&
                       std::isdigit(static_cast<unsigned char>(fileName[4])) &&
                       std::isdigit(static_cast<unsigned char>(fileName[5]));
            }

            int parseMoSessionId(const char* fileName)
            {
                if (!hasMoCsvName(fileName))
                    return -1;

                char digits[4] = {fileName[3], fileName[4], fileName[5], '\0'};
                return std::atoi(digits);
            }
        } // namespace

        const char* safetyStateToString(SafetyState state)
        {
            switch (state)
            {
            case SafetyState::SafeDisabled:
                return "SafeDisabled";
            case SafetyState::OutputArmed:
                return "OutputArmed";
            case SafetyState::OutputEnabled:
                return "OutputEnabled";
            case SafetyState::Fault:
                return "Fault";
            case SafetyState::FaultCleared:
                return "SafeDisabled";
            }
            return "Unknown";
        }

        const char* outputPatternFromAppliedTarget(int appliedTargetPercent, bool physicalBackendAvailable)
        {
            if (!physicalBackendAvailable)
                return "NOT_APPLICABLE";

            if (appliedTargetPercent > 0)
                return "GPIO9_PWM_GPIO8_LOW";

            if (appliedTargetPercent < 0)
                return "GPIO9_LOW_GPIO8_PWM";

            return "LOW_LOW";
        }

        int gpio9DutyPercentFromAppliedTarget(int appliedTargetPercent, bool physicalBackendAvailable)
        {
            if (!physicalBackendAvailable || appliedTargetPercent <= 0)
                return 0;

            return std::min(SafetyController::kMaxTargetPercent, appliedTargetPercent);
        }

        int gpio8DutyPercentFromAppliedTarget(int appliedTargetPercent, bool physicalBackendAvailable)
        {
            if (!physicalBackendAvailable || appliedTargetPercent >= 0)
                return 0;

            return std::min(SafetyController::kMaxTargetPercent, -appliedTargetPercent);
        }

        std::string headerLine() { return kCsvHeader; }

        std::string formatRow(const Row& row)
        {
            std::ostringstream out;
            out.setf(std::ios::fixed);

            out << kSchemaVersion << ',';
            out << row.sessionId << ',';
            out << row.sampleIndex << ',';
            out << row.elapsedMs << ',';
            out.precision(3);
            out << (static_cast<double>(row.elapsedMs) / 1000.0) << ',';
            out << row.event << ',';
            out << safetyStateToString(row.safetyState) << ',';
            out << (row.physicalOutputAllowed ? 1 : 0) << ',';
            out << row.requestedTargetPercent << ',';
            out << row.appliedTargetPercent << ',';
            out << outputPatternFromAppliedTarget(row.appliedTargetPercent, row.physicalBackendAvailable) << ',';
            out << gpio9DutyPercentFromAppliedTarget(row.appliedTargetPercent, row.physicalBackendAvailable) << ',';
            out << gpio8DutyPercentFromAppliedTarget(row.appliedTargetPercent, row.physicalBackendAvailable) << ',';
            out << row.measurementPathCode << ',';
            out << row.voltageSemantics << ',';
            out << row.currentSemantics << ',';
            out << row.powerSemantics << ',';
            out.precision(3);
            out << row.measurement.voltageV << ',';
            out.precision(6);
            out << row.measurement.currentA << ',';
            out << row.measurement.powerW << ',';
            out << row.powerSource << ',';
            out << (row.pwmWaveformCaptured ? 1 : 0) << ',';
            out << (row.abnormalFlag ? 1 : 0) << ',';
            out << escapeCsvField(row.note);

            return out.str();
        }

        bool FileLogger::begin(const Row& startRow)
        {
            if (_file != nullptr)
                return true;

            if (!_ensureRecordFolder())
                return false;

            const int nextId = _findMaxSessionId() + 1;
            _fileName = _createFileName(nextId);
            _filePath = _createFilePath(_fileName);
            _sessionId = _fileName.substr(0, _fileName.size() - 4);
            _sampleIndex = 0;

            _file = std::fopen(_filePath.c_str(), "w");
            if (_file == nullptr)
                return false;

            _writeLine(headerLine());

            Row row = startRow;
            row.sessionId = _sessionId;
            row.sampleIndex = nextSampleIndex();
            row.event = "start";
            _writeLine(formatRow(row));
            return true;
        }

        void FileLogger::append(const Row& row)
        {
            if (_file == nullptr)
                return;

            Row rowToWrite = row;
            rowToWrite.sessionId = _sessionId;
            rowToWrite.sampleIndex = nextSampleIndex();
            _writeLine(formatRow(rowToWrite));
        }

        void FileLogger::stop(const Row& stopRow)
        {
            if (_file == nullptr)
                return;

            Row row = stopRow;
            row.sessionId = _sessionId;
            row.sampleIndex = nextSampleIndex();
            row.event = "stop";
            _writeLine(formatRow(row));
            std::fclose(_file);
            _file = nullptr;
        }

        bool FileLogger::isOpen() const { return _file != nullptr; }

        const std::string& FileLogger::fileName() const { return _fileName; }

        const std::string& FileLogger::filePath() const { return _filePath; }

        const std::string& FileLogger::sessionId() const { return _sessionId; }

        uint32_t FileLogger::nextSampleIndex() { return _sampleIndex++; }

        bool FileLogger::_ensureRecordFolder()
        {
            const std::string folderPath = _recordFolderPath();
            DIR* folder = opendir(folderPath.c_str());
            if (folder != nullptr)
            {
                closedir(folder);
                return true;
            }

            return mkdir(folderPath.c_str(), S_IRWXU) == 0;
        }

        int FileLogger::_findMaxSessionId()
        {
            int maxId = -1;
            DIR* folder = opendir(_recordFolderPath().c_str());
            if (folder == nullptr)
                return maxId;

            struct dirent* entry = nullptr;
            while ((entry = readdir(folder)) != nullptr)
                maxId = std::max(maxId, parseMoSessionId(entry->d_name));

            closedir(folder);
            return maxId;
        }

        std::string FileLogger::_recordFolderPath()
        {
#if defined(ESP_PLATFORM)
            return "/spiflash/rec";
#else
            return ".";
#endif
        }

        std::string FileLogger::_createFileName(int sessionId)
        {
            char fileName[16];
            std::snprintf(fileName, sizeof(fileName), "MO-%03d.csv", sessionId);
            return fileName;
        }

        std::string FileLogger::_createFilePath(const std::string& fileName)
        {
            return _recordFolderPath() + "/" + fileName;
        }

        void FileLogger::_writeLine(const std::string& line)
        {
            if (_file == nullptr)
                return;

            std::fputs(line.c_str(), _file);
            std::fputc('\n', _file);
            std::fflush(_file);
        }
    } // namespace CSV
} // namespace MOTOR_OBSERVE
