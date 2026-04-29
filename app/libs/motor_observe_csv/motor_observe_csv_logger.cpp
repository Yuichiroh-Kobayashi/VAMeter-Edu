/*
 * SPDX-License-Identifier: MIT
 */
#include "motor_observe_csv_logger.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <cstdio>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(__has_include)
#if __has_include(<sys/statvfs.h>)
#include <sys/statvfs.h>
#define MOTOR_OBSERVE_CSV_HAS_STATVFS 1
#endif
#endif

#ifndef MOTOR_OBSERVE_CSV_DISABLE_SPDLOG
#if defined(ESP_PLATFORM)
#include "esp_log.h"
static const char* kMotorObserveCsvLogTag = "MO_CSV";
#define MO_CSV_LOG_INFO(...) ESP_LOGI(kMotorObserveCsvLogTag, __VA_ARGS__)
#define MO_CSV_LOG_WARN(...) ESP_LOGW(kMotorObserveCsvLogTag, __VA_ARGS__)
#define MO_CSV_LOG_ERROR(...) ESP_LOGE(kMotorObserveCsvLogTag, __VA_ARGS__)
#else
#define MO_CSV_LOG_INFO(...)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        std::printf("[MO_CSV] ");                                                                                      \
        std::printf(__VA_ARGS__);                                                                                      \
        std::printf("\n");                                                                                            \
    } while (false)
#define MO_CSV_LOG_WARN(...)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        std::printf("[MO_CSV][WARN] ");                                                                                \
        std::printf(__VA_ARGS__);                                                                                      \
        std::printf("\n");                                                                                            \
    } while (false)
#define MO_CSV_LOG_ERROR(...)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        std::printf("[MO_CSV][ERROR] ");                                                                               \
        std::printf(__VA_ARGS__);                                                                                      \
        std::printf("\n");                                                                                            \
    } while (false)
#endif
#else
#define MO_CSV_LOG_INFO(...)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (false)
#define MO_CSV_LOG_WARN(...)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (false)
#define MO_CSV_LOG_ERROR(...)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (false)
#endif

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

            bool hasRecCsvName(const char* fileName)
            {
                if (fileName == nullptr)
                    return false;

                const size_t length = std::strlen(fileName);
                if (length != std::strlen("REC-000.csv"))
                    return false;

                if (std::strncmp(fileName, "REC-", 4) != 0)
                    return false;

                if (std::strcmp(fileName + length - 4, ".csv") != 0)
                    return false;

                return std::isdigit(static_cast<unsigned char>(fileName[4])) &&
                       std::isdigit(static_cast<unsigned char>(fileName[5])) &&
                       std::isdigit(static_cast<unsigned char>(fileName[6]));
            }

            bool isDotEntry(const char* fileName)
            {
                return fileName != nullptr && (std::strcmp(fileName, ".") == 0 || std::strcmp(fileName, "..") == 0);
            }

            std::string joinPath(const std::string& folderPath, const char* fileName)
            {
                return folderPath + "/" + (fileName == nullptr ? "" : fileName);
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
            MO_CSV_LOG_INFO("csv begin start");
            MO_CSV_LOG_INFO("record folder path: %s", _recordFolderPath().c_str());

            if (_file != nullptr)
            {
                MO_CSV_LOG_INFO("csv begin success: already open file name=%s file path=%s", _fileName.c_str(), _filePath.c_str());
                return true;
            }

            if (!_ensureRecordFolder())
            {
                MO_CSV_LOG_ERROR("csv begin failed: record folder prepare failed");
                return false;
            }
            MO_CSV_LOG_INFO("csv begin folder prepare: success");

            const int nextId = _findMaxSessionId() + 1;
            MO_CSV_LOG_INFO("csv begin next session id: %d", nextId);
            _fileName = _createFileName(nextId);
            _filePath = _createFilePath(_fileName);
            _sessionId = _fileName.substr(0, _fileName.size() - 4);
            _sampleIndex = 0;
            MO_CSV_LOG_INFO("csv begin file name: %s", _fileName.c_str());
            MO_CSV_LOG_INFO("csv begin file path: %s", _filePath.c_str());
            _logCandidateFileStat(_filePath);

            errno = 0;
            _file = std::fopen(_filePath.c_str(), "w");
            if (_file == nullptr)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("csv begin fopen failed: path=%s errno=%d message=%s",
                                 _filePath.c_str(),
                                 savedErrno,
                                 std::strerror(savedErrno));
                _logStorageDiagnostics(_filePath);
                return false;
            }
            MO_CSV_LOG_INFO("csv begin fopen: success");

            if (!_writeLine("header", headerLine()))
            {
                std::fclose(_file);
                _file = nullptr;
                MO_CSV_LOG_ERROR("csv begin failed: write header failed");
                return false;
            }
            MO_CSV_LOG_INFO("csv begin write header: success");

            Row row = startRow;
            row.sessionId = _sessionId;
            row.sampleIndex = nextSampleIndex();
            row.event = "start";
            if (!_writeLine("start", formatRow(row)))
            {
                std::fclose(_file);
                _file = nullptr;
                MO_CSV_LOG_ERROR("csv begin failed: write start failed");
                return false;
            }
            MO_CSV_LOG_INFO("csv begin write start: success");
            MO_CSV_LOG_INFO("csv begin success: file name=%s file path=%s", _fileName.c_str(), _filePath.c_str());

            return true;
        }

        bool FileLogger::append(const Row& row)
        {
            if (_file == nullptr)
                return false;

            Row rowToWrite = row;
            rowToWrite.sessionId = _sessionId;
            rowToWrite.sampleIndex = nextSampleIndex();
            return _writeLine(rowToWrite.event, formatRow(rowToWrite));
        }

        bool FileLogger::stop(const Row& stopRow)
        {
            if (_file == nullptr)
                return false;

            Row row = stopRow;
            row.sessionId = _sessionId;
            row.sampleIndex = nextSampleIndex();
            row.event = "stop";
            const bool wrote = _writeLine("stop", formatRow(row));
            errno = 0;
            const bool closed = (std::fclose(_file) == 0);
            if (!closed)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("csv stop fclose failed: file path=%s errno=%d message=%s",
                                 _filePath.c_str(),
                                 savedErrno,
                                 std::strerror(savedErrno));
            }
            _file = nullptr;
            return wrote && closed;
        }

        bool FileLogger::isOpen() const { return _file != nullptr; }

        const std::string& FileLogger::fileName() const { return _fileName; }

        const std::string& FileLogger::filePath() const { return _filePath; }

        const std::string& FileLogger::sessionId() const { return _sessionId; }

        uint32_t FileLogger::nextSampleIndex() { return _sampleIndex++; }

        bool FileLogger::_ensureRecordFolder()
        {
            const std::string folderPath = _recordFolderPath();
            MO_CSV_LOG_INFO("csv ensure record folder path: %s", folderPath.c_str());
            errno = 0;
            DIR* folder = opendir(folderPath.c_str());
            if (folder != nullptr)
            {
                closedir(folder);
                MO_CSV_LOG_INFO("csv ensure record folder opendir success: %s", folderPath.c_str());
                return true;
            }

            int savedErrno = errno;
            MO_CSV_LOG_WARN("csv ensure record folder opendir failed: path=%s errno=%d message=%s",
                            folderPath.c_str(),
                            savedErrno,
                            std::strerror(savedErrno));

            MO_CSV_LOG_INFO("csv ensure record folder mkdir start: %s", folderPath.c_str());
            errno = 0;
            if (mkdir(folderPath.c_str(), S_IRWXU) == 0)
            {
                MO_CSV_LOG_INFO("csv ensure record folder mkdir success: %s", folderPath.c_str());
                return true;
            }

            savedErrno = errno;
            MO_CSV_LOG_WARN("csv ensure record folder mkdir failed: path=%s errno=%d message=%s",
                            folderPath.c_str(),
                            savedErrno,
                            std::strerror(savedErrno));

            if (savedErrno == EEXIST)
            {
                errno = 0;
                folder = opendir(folderPath.c_str());
                if (folder != nullptr)
                {
                    closedir(folder);
                    MO_CSV_LOG_INFO("csv ensure record folder success after EEXIST: %s", folderPath.c_str());
                    return true;
                }

                savedErrno = errno;
                MO_CSV_LOG_ERROR("csv ensure record folder opendir after EEXIST failed: path=%s errno=%d message=%s",
                                 folderPath.c_str(),
                                 savedErrno,
                                 std::strerror(savedErrno));
            }

            return false;
        }

        int FileLogger::_findMaxSessionId()
        {
            int maxId = -1;
            const std::string folderPath = _recordFolderPath();
            MO_CSV_LOG_INFO("csv find max session id: record folder path=%s", folderPath.c_str());
            errno = 0;
            DIR* folder = opendir(folderPath.c_str());
            if (folder == nullptr)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_WARN("csv find max session id opendir failed: path=%s errno=%d message=%s",
                                folderPath.c_str(),
                                savedErrno,
                                std::strerror(savedErrno));
                return maxId;
            }

            struct dirent* entry = nullptr;
            int scannedCount = 0;
            int matchedCount = 0;
            int ignoredCount = 0;
            while ((entry = readdir(folder)) != nullptr)
            {
                ++scannedCount;
                const int sessionId = parseMoSessionId(entry->d_name);
                if (sessionId >= 0)
                {
                    ++matchedCount;
                    maxId = std::max(maxId, sessionId);
                }
                else
                {
                    ++ignoredCount;
                }
            }

            closedir(folder);
            MO_CSV_LOG_INFO("csv find max session id: scanned=%d matched=%d ignored=%d max=%d",
                            scannedCount,
                            matchedCount,
                            ignoredCount,
                            maxId);
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

        void FileLogger::_logStorageDiagnostics(const std::string& candidateFilePath)
        {
            MO_CSV_LOG_INFO("record folder diagnostics: start");
            _logCandidateFileStat(candidateFilePath);
            _logRecordFolderDiagnostics();
            _logStatvfsDiagnostics("/spiflash");
            _logStatvfsDiagnostics(_recordFolderPath());
            _logProbeCreateDiagnostics();
            MO_CSV_LOG_INFO("record folder diagnostics: end");
        }

        void FileLogger::_logCandidateFileStat(const std::string& filePath)
        {
            struct stat fileStat;
            errno = 0;
            if (stat(filePath.c_str(), &fileStat) == 0)
            {
                MO_CSV_LOG_INFO("csv candidate file stat: path=%s exists=1 size=%lld",
                                filePath.c_str(),
                                static_cast<long long>(fileStat.st_size));
                return;
            }

            const int savedErrno = errno;
            MO_CSV_LOG_INFO("csv candidate file stat: path=%s exists=0 errno=%d message=%s",
                            filePath.c_str(),
                            savedErrno,
                            std::strerror(savedErrno));
        }

        void FileLogger::_logRecordFolderDiagnostics()
        {
            struct LargestFile
            {
                std::string name;
                long long size = 0;
            };

            const std::string folderPath = _recordFolderPath();
            MO_CSV_LOG_INFO("record folder diagnostics: folder=%s", folderPath.c_str());
            errno = 0;
            DIR* folder = opendir(folderPath.c_str());
            if (folder == nullptr)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("record folder diagnostics opendir failed: path=%s errno=%d message=%s",
                                 folderPath.c_str(),
                                 savedErrno,
                                 std::strerror(savedErrno));
                return;
            }

            int totalEntryCount = 0;
            int moCsvCount = 0;
            int recCsvCount = 0;
            int otherCount = 0;
            int zeroByteFileCount = 0;
            int statFailedCount = 0;
            long long totalRegularSize = 0;
            long long moCsvSize = 0;
            long long recCsvSize = 0;
            std::vector<LargestFile> largestFiles;

            struct dirent* entry = nullptr;
            while ((entry = readdir(folder)) != nullptr)
            {
                if (isDotEntry(entry->d_name))
                    continue;

                ++totalEntryCount;
                const bool isMoCsv = hasMoCsvName(entry->d_name);
                const bool isRecCsv = hasRecCsvName(entry->d_name);
                if (isMoCsv)
                    ++moCsvCount;
                else if (isRecCsv)
                    ++recCsvCount;
                else
                    ++otherCount;

                const std::string filePath = joinPath(folderPath, entry->d_name);
                struct stat fileStat;
                errno = 0;
                if (stat(filePath.c_str(), &fileStat) != 0)
                {
                    ++statFailedCount;
                    const int savedErrno = errno;
                    MO_CSV_LOG_WARN("record folder diagnostics stat failed: name=%s errno=%d message=%s",
                                    entry->d_name,
                                    savedErrno,
                                    std::strerror(savedErrno));
                    continue;
                }

                if (!S_ISREG(fileStat.st_mode))
                    continue;

                const long long size = static_cast<long long>(fileStat.st_size);
                totalRegularSize += size;
                if (isMoCsv)
                    moCsvSize += size;
                if (isRecCsv)
                    recCsvSize += size;
                if (size == 0)
                    ++zeroByteFileCount;

                largestFiles.push_back({entry->d_name, size});
            }

            closedir(folder);

            std::sort(largestFiles.begin(), largestFiles.end(), [](const LargestFile& lhs, const LargestFile& rhs) {
                return lhs.size > rhs.size;
            });

            MO_CSV_LOG_INFO("record folder diagnostics counts: total=%d mo_csv=%d rec_csv=%d other=%d zero_byte=%d stat_failed=%d",
                            totalEntryCount,
                            moCsvCount,
                            recCsvCount,
                            otherCount,
                            zeroByteFileCount,
                            statFailedCount);
            MO_CSV_LOG_INFO("record folder diagnostics sizes: total_regular=%lld mo_csv=%lld rec_csv=%lld",
                            totalRegularSize,
                            moCsvSize,
                            recCsvSize);

            const size_t topCount = std::min<size_t>(5, largestFiles.size());
            for (size_t index = 0; index < topCount; ++index)
            {
                MO_CSV_LOG_INFO("record folder diagnostics largest[%u]: name=%s size=%lld",
                                static_cast<unsigned>(index + 1),
                                largestFiles[index].name.c_str(),
                                largestFiles[index].size);
            }
        }

        void FileLogger::_logStatvfsDiagnostics(const std::string& path)
        {
#if defined(MOTOR_OBSERVE_CSV_HAS_STATVFS)
            struct statvfs fsStat;
            errno = 0;
            if (statvfs(path.c_str(), &fsStat) != 0)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_WARN("storage statvfs failed: path=%s errno=%d message=%s",
                                path.c_str(),
                                savedErrno,
                                std::strerror(savedErrno));
                return;
            }

            const unsigned long long blockSize = static_cast<unsigned long long>(fsStat.f_bsize);
            const unsigned long long totalBytes = blockSize * static_cast<unsigned long long>(fsStat.f_blocks);
            const unsigned long long freeBytes = blockSize * static_cast<unsigned long long>(fsStat.f_bfree);
            const unsigned long long availableBytes = blockSize * static_cast<unsigned long long>(fsStat.f_bavail);
            MO_CSV_LOG_INFO("storage statvfs: path=%s f_bsize=%llu f_blocks=%llu f_bfree=%llu f_bavail=%llu total_bytes=%llu free_bytes=%llu available_bytes=%llu",
                            path.c_str(),
                            blockSize,
                            static_cast<unsigned long long>(fsStat.f_blocks),
                            static_cast<unsigned long long>(fsStat.f_bfree),
                            static_cast<unsigned long long>(fsStat.f_bavail),
                            totalBytes,
                            freeBytes,
                            availableBytes);
#else
            MO_CSV_LOG_WARN("storage statvfs unavailable: path=%s", path.c_str());
#endif
        }

        void FileLogger::_logProbeCreateDiagnostics()
        {
#if defined(ESP_PLATFORM)
            MO_CSV_LOG_INFO("probe create diagnostics: start");
            const std::string recordFolder = _recordFolderPath();
            const std::string probeMoPath = recordFolder + "/MO-PROBE.tmp";
            const std::string probeTxtPath = recordFolder + "/TEST.TXT";
            _logSingleProbeCreate(probeMoPath, "w");
            _logSingleProbeCreate(probeMoPath, "wb");
            _logSingleProbeCreate(probeTxtPath, "w");
            _logSingleProbeCreate(probeTxtPath, "wb");
            _logSingleProbeCreate("/spiflash/MO-PROBE.tmp", "w");
            _logSingleProbeCreate("/spiflash/MO-PROBE.tmp", "wb");
            MO_CSV_LOG_INFO("probe create diagnostics: end");
#else
            MO_CSV_LOG_INFO("probe create diagnostics: skipped on non-ESP platform");
#endif
        }

        void FileLogger::_logSingleProbeCreate(const std::string& path, const char* mode)
        {
            MO_CSV_LOG_INFO("probe create: path=%s mode=%s start", path.c_str(), mode);
            errno = 0;
            FILE* probeFile = std::fopen(path.c_str(), mode);
            if (probeFile == nullptr)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("probe create fopen failed: path=%s mode=%s errno=%d message=%s",
                                 path.c_str(),
                                 mode,
                                 savedErrno,
                                 std::strerror(savedErrno));
                return;
            }

            errno = 0;
            const bool wrote = (std::fputs("motor_observe_probe\n", probeFile) >= 0);
            if (!wrote)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("probe create write failed: path=%s mode=%s errno=%d message=%s",
                                 path.c_str(),
                                 mode,
                                 savedErrno,
                                 std::strerror(savedErrno));
            }

            errno = 0;
            const bool flushed = (std::fflush(probeFile) == 0);
            if (!flushed)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("probe create fflush failed: path=%s mode=%s errno=%d message=%s",
                                 path.c_str(),
                                 mode,
                                 savedErrno,
                                 std::strerror(savedErrno));
            }

            errno = 0;
            const bool closed = (std::fclose(probeFile) == 0);
            if (!closed)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("probe create fclose failed: path=%s mode=%s errno=%d message=%s",
                                 path.c_str(),
                                 mode,
                                 savedErrno,
                                 std::strerror(savedErrno));
            }

            struct stat fileStat;
            errno = 0;
            if (stat(path.c_str(), &fileStat) == 0)
            {
                MO_CSV_LOG_INFO("probe create stat: path=%s mode=%s size=%lld",
                                path.c_str(),
                                mode,
                                static_cast<long long>(fileStat.st_size));
            }
            else
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("probe create stat failed: path=%s mode=%s errno=%d message=%s",
                                 path.c_str(),
                                 mode,
                                 savedErrno,
                                 std::strerror(savedErrno));
            }

            errno = 0;
            if (std::remove(path.c_str()) == 0)
            {
                MO_CSV_LOG_INFO("probe create remove success: path=%s mode=%s", path.c_str(), mode);
            }
            else
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("probe create remove failed: path=%s mode=%s errno=%d message=%s",
                                 path.c_str(),
                                 mode,
                                 savedErrno,
                                 std::strerror(savedErrno));
            }
        }

        bool FileLogger::_writeLine(const std::string& label, const std::string& line)
        {
            if (_file == nullptr)
            {
                MO_CSV_LOG_ERROR("csv write %s failed: file is not open", label.c_str());
                return false;
            }

            errno = 0;
            if (std::fputs(line.c_str(), _file) < 0)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("csv write %s fputs failed: errno=%d message=%s",
                                 label.c_str(),
                                 savedErrno,
                                 std::strerror(savedErrno));
                return false;
            }

            errno = 0;
            if (std::fputc('\n', _file) == EOF)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("csv write %s fputc failed: errno=%d message=%s",
                                 label.c_str(),
                                 savedErrno,
                                 std::strerror(savedErrno));
                return false;
            }

            errno = 0;
            if (std::fflush(_file) != 0)
            {
                const int savedErrno = errno;
                MO_CSV_LOG_ERROR("csv write %s fflush failed: errno=%d message=%s",
                                 label.c_str(),
                                 savedErrno,
                                 std::strerror(savedErrno));
                return false;
            }

            return true;
        }
    } // namespace CSV
} // namespace MOTOR_OBSERVE
