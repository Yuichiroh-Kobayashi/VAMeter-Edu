/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdio>
#include <string>

#include "../motor_observe_backend/motor_observe_backend.h"

namespace MOTOR_OBSERVE
{
    namespace CSV
    {
        static constexpr const char* kSchemaVersion = "motor_observe_csv_v0.1";
        static constexpr uint32_t kDefaultSampleIntervalMs = 100;

        struct MeasurementSnapshot
        {
            float voltageV = 0.0f;
            float currentA = 0.0f;
            float powerW = 0.0f;
        };

        struct Row
        {
            std::string sessionId;
            uint32_t sampleIndex = 0;
            uint32_t elapsedMs = 0;
            std::string event = "sample";
            SafetyState safetyState = SafetyState::SafeDisabled;
            bool physicalOutputAllowed = false;
            int requestedTargetPercent = 0;
            int appliedTargetPercent = 0;
            std::string measurementPathCode = "unknown";
            std::string voltageSemantics = "unknown";
            std::string currentSemantics = "unknown";
            std::string powerSemantics = "unknown";
            MeasurementSnapshot measurement;
            std::string powerSource = "hal";
            bool pwmWaveformCaptured = false;
            bool abnormalFlag = false;
            std::string note;
            bool physicalBackendAvailable = true;
        };

        const char* safetyStateToString(SafetyState state);
        const char* outputPatternFromAppliedTarget(int appliedTargetPercent, bool physicalBackendAvailable);
        int gpio9DutyPercentFromAppliedTarget(int appliedTargetPercent, bool physicalBackendAvailable);
        int gpio8DutyPercentFromAppliedTarget(int appliedTargetPercent, bool physicalBackendAvailable);
        std::string headerLine();
        std::string formatRow(const Row& row);

        class FileLogger
        {
        public:
            bool begin(const Row& startRow);
            void append(const Row& row);
            void stop(const Row& stopRow);
            bool isOpen() const;
            const std::string& fileName() const;
            const std::string& filePath() const;
            const std::string& sessionId() const;
            uint32_t nextSampleIndex();

        private:
            FILE* _file = nullptr;
            std::string _fileName;
            std::string _filePath;
            std::string _sessionId;
            uint32_t _sampleIndex = 0;

            static bool _ensureRecordFolder();
            static int _findMaxSessionId();
            static std::string _recordFolderPath();
            static std::string _createFileName(int sessionId);
            static std::string _createFilePath(const std::string& fileName);
            void _writeLine(const std::string& line);
        };
    } // namespace CSV
} // namespace MOTOR_OBSERVE
