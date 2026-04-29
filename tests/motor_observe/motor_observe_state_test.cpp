/*
 * SPDX-License-Identifier: MIT
 */
#include "libs/motor_observe_backend/motor_observe_backend.h"
#include "libs/motor_observe_csv/motor_observe_csv_logger.h"
#include "libs/local_csv_download/local_csv_download_name.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using MOTOR_OBSERVE::Backend;
using MOTOR_OBSERVE::BackendController;
using MOTOR_OBSERVE::NoopBackend;
using MOTOR_OBSERVE::SafetyController;
using MOTOR_OBSERVE::SafetyState;

namespace
{
    int g_failureCount = 0;

    void check(bool condition, const char* expression, int line)
    {
        if (condition)
            return;

        std::cerr << "FAIL line " << line << ": " << expression << std::endl;
        ++g_failureCount;
    }

#define CHECK(expr) check((expr), #expr, __LINE__)

    class FaultBackend : public Backend
    {
    public:
        bool begin() override
        {
            disarm();
            return true;
        }

        void disarm() override { _lastAppliedTargetPercent = 0; }

        void setTargetPercent(int targetPercent) override { _lastAppliedTargetPercent = targetPercent; }

        void update() override {}

        bool hasFault() const override { return _hasFault; }

        const std::string& getFaultReason() const override { return _faultReason; }

        int getLastAppliedTargetPercent() const override { return _lastAppliedTargetPercent; }

        void triggerFault(const std::string& reason)
        {
            _hasFault = true;
            _faultReason = reason;
        }

    private:
        bool _hasFault = false;
        int _lastAppliedTargetPercent = 0;
        std::string _faultReason;
    };

    std::vector<std::string> splitCsvLine(const std::string& line)
    {
        std::vector<std::string> fields;
        std::stringstream stream(line);
        std::string field;
        while (std::getline(stream, field, ','))
            fields.push_back(field);
        return fields;
    }

    std::string readWholeFile(const std::string& filePath)
    {
        std::ifstream input(filePath.c_str());
        std::stringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    void testSafetyController()
    {
        SafetyController safetyController;

        CHECK(safetyController.getState() == SafetyState::SafeDisabled);
        CHECK(safetyController.getTargetPercent() == 0);
        CHECK(!safetyController.isPhysicalOutputAllowed());
        CHECK(!safetyController.enableOutput());
        CHECK(safetyController.getState() == SafetyState::SafeDisabled);

        CHECK(safetyController.prepareOutput());
        CHECK(safetyController.getState() == SafetyState::OutputArmed);
        CHECK(!safetyController.isPhysicalOutputAllowed());

        CHECK(safetyController.enableOutput());
        CHECK(safetyController.getState() == SafetyState::OutputEnabled);
        CHECK(safetyController.isPhysicalOutputAllowed());

        safetyController.setTargetPercent(150);
        CHECK(safetyController.getTargetPercent() == 100);
        safetyController.setTargetPercent(-150);
        CHECK(safetyController.getTargetPercent() == -100);

        safetyController.disableOutput();
        CHECK(safetyController.getState() == SafetyState::SafeDisabled);
        CHECK(safetyController.getTargetPercent() == 0);
        CHECK(!safetyController.isPhysicalOutputAllowed());

        safetyController.prepareOutput();
        safetyController.enableOutput();
        safetyController.setTargetPercent(70);
        safetyController.leaveMode();
        CHECK(safetyController.getState() == SafetyState::SafeDisabled);
        CHECK(safetyController.getTargetPercent() == 0);

        safetyController.prepareOutput();
        safetyController.enableOutput();
        safetyController.setTargetPercent(70);
        safetyController.timeout();
        CHECK(safetyController.getState() == SafetyState::SafeDisabled);
        CHECK(safetyController.getTargetPercent() == 0);

        safetyController.prepareOutput();
        safetyController.enableOutput();
        safetyController.setTargetPercent(80);
        safetyController.setFault("test fault");
        CHECK(safetyController.getState() == SafetyState::Fault);
        CHECK(safetyController.getTargetPercent() == 0);
        CHECK(!safetyController.isPhysicalOutputAllowed());
        CHECK(safetyController.getFaultReason() == "test fault");
        safetyController.setTargetPercent(50);
        CHECK(safetyController.getTargetPercent() == 0);
        CHECK(safetyController.clearFault());
        CHECK(safetyController.getState() == SafetyState::SafeDisabled);
        CHECK(safetyController.getTargetPercent() == 0);
    }

    void testBackendControllerWithNoopBackend()
    {
        SafetyController safetyController;
        NoopBackend backend;
        BackendController controller(safetyController, backend);

        CHECK(controller.begin());
        CHECK(controller.getState() == SafetyState::SafeDisabled);
        CHECK(controller.getLastAppliedTargetPercent() == 0);

        controller.setTargetPercent(50);
        controller.update();
        CHECK(controller.getTargetPercent() == 50);
        CHECK(controller.getLastAppliedTargetPercent() == 0);

        CHECK(controller.prepareOutput());
        controller.update();
        CHECK(controller.getState() == SafetyState::OutputArmed);
        CHECK(controller.getLastAppliedTargetPercent() == 0);

        controller.setTargetPercent(50);
        CHECK(controller.enableOutput());
        controller.update();
        CHECK(controller.getState() == SafetyState::OutputEnabled);
        CHECK(controller.getLastAppliedTargetPercent() == 50);

        controller.disableOutput();
        CHECK(controller.getState() == SafetyState::SafeDisabled);
        CHECK(controller.getLastAppliedTargetPercent() == 0);

        CHECK(controller.prepareOutput());
        controller.setTargetPercent(60);
        CHECK(controller.enableOutput());
        controller.update();
        CHECK(controller.getLastAppliedTargetPercent() == 60);
        controller.timeout();
        CHECK(controller.getState() == SafetyState::SafeDisabled);
        CHECK(controller.getLastAppliedTargetPercent() == 0);

        CHECK(controller.prepareOutput());
        controller.setTargetPercent(70);
        CHECK(controller.enableOutput());
        controller.update();
        CHECK(controller.getLastAppliedTargetPercent() == 70);
        controller.leaveMode();
        CHECK(controller.getState() == SafetyState::SafeDisabled);
        CHECK(controller.getLastAppliedTargetPercent() == 0);

        CHECK(controller.prepareOutput());
        controller.setTargetPercent(80);
        CHECK(controller.enableOutput());
        controller.update();
        CHECK(controller.getLastAppliedTargetPercent() == 80);
        controller.setFault("manual fault");
        CHECK(controller.getState() == SafetyState::Fault);
        CHECK(controller.getLastAppliedTargetPercent() == 0);
        controller.setTargetPercent(50);
        controller.update();
        CHECK(controller.getTargetPercent() == 0);
        CHECK(controller.getLastAppliedTargetPercent() == 0);

        CHECK(controller.clearFault());
        CHECK(controller.prepareOutput());
        controller.setTargetPercent(-150);
        CHECK(controller.enableOutput());
        controller.update();
        CHECK(controller.getLastAppliedTargetPercent() == -100);
    }

    void testBackendControllerWithFaultBackend()
    {
        SafetyController safetyController;
        FaultBackend backend;
        BackendController controller(safetyController, backend);

        CHECK(controller.begin());
        CHECK(controller.prepareOutput());
        controller.setTargetPercent(40);
        CHECK(controller.enableOutput());
        controller.update();
        CHECK(controller.getLastAppliedTargetPercent() == 40);

        backend.triggerFault("backend fault");
        controller.update();
        CHECK(controller.getState() == SafetyState::Fault);
        CHECK(!controller.isPhysicalOutputAllowed());
        CHECK(controller.getLastAppliedTargetPercent() == 0);
        CHECK(controller.getFaultReason() == "backend fault");

        controller.setTargetPercent(60);
        controller.update();
        CHECK(controller.getTargetPercent() == 0);
        CHECK(controller.getLastAppliedTargetPercent() == 0);
    }

    void testCsvHeaderAndRows()
    {
        const std::string expectedHeader =
            "schema_version,session_id,sample_index,t_ms,t_s,event,safety_state,physical_output_allowed,"
            "requested_target_percent,applied_target_percent,output_pattern,gpio9_duty_percent,"
            "gpio8_duty_percent,measurement_path_code,voltage_semantics,current_semantics,power_semantics,"
            "voltage_v,current_a,power_w,power_source,pwm_waveform_captured,abnormal_flag,note";
        CHECK(MOTOR_OBSERVE::CSV::headerLine() == expectedHeader);

        MOTOR_OBSERVE::CSV::Row row;
        row.sessionId = "MO-000";
        row.sampleIndex = 12;
        row.elapsedMs = 1200;
        row.event = "sample";
        row.safetyState = SafetyState::SafeDisabled;
        row.physicalOutputAllowed = false;
        row.requestedTargetPercent = 30;
        row.appliedTargetPercent = 0;
        row.measurementPathCode = "unknown";
        row.voltageSemantics = "unknown";
        row.currentSemantics = "unknown";
        row.powerSemantics = "unknown";
        row.measurement.voltageV = 2.84f;
        row.measurement.currentA = 0.132f;
        row.measurement.powerW = 0.37488f;
        row.powerSource = "hal";
        row.note = "test";
        row.physicalBackendAvailable = true;

        std::vector<std::string> fields = splitCsvLine(MOTOR_OBSERVE::CSV::formatRow(row));
        CHECK(fields.size() == 24);
        CHECK(fields[0] == "motor_observe_csv_v0.1");
        CHECK(fields[1] == "MO-000");
        CHECK(fields[6] == "SafeDisabled");
        CHECK(fields[7] == "0");
        CHECK(fields[8] == "30");
        CHECK(fields[9] == "0");
        CHECK(fields[10] == "LOW_LOW");
        CHECK(fields[11] == "0");
        CHECK(fields[12] == "0");
        CHECK(!fields[13].empty());

        row.safetyState = SafetyState::OutputArmed;
        row.physicalOutputAllowed = false;
        row.requestedTargetPercent = 30;
        row.appliedTargetPercent = 0;
        fields = splitCsvLine(MOTOR_OBSERVE::CSV::formatRow(row));
        CHECK(fields[6] == "OutputArmed");
        CHECK(fields[7] == "0");
        CHECK(fields[9] == "0");
        CHECK(fields[10] == "LOW_LOW");

        row.safetyState = SafetyState::OutputEnabled;
        row.physicalOutputAllowed = true;
        row.requestedTargetPercent = 30;
        row.appliedTargetPercent = 30;
        fields = splitCsvLine(MOTOR_OBSERVE::CSV::formatRow(row));
        CHECK(fields[10] == "GPIO9_PWM_GPIO8_LOW");
        CHECK(fields[11] == "30");
        CHECK(fields[12] == "0");

        row.requestedTargetPercent = -30;
        row.appliedTargetPercent = -30;
        fields = splitCsvLine(MOTOR_OBSERVE::CSV::formatRow(row));
        CHECK(fields[10] == "GPIO9_LOW_GPIO8_PWM");
        CHECK(fields[11] == "0");
        CHECK(fields[12] == "30");

        row.safetyState = SafetyState::Fault;
        row.physicalOutputAllowed = false;
        row.requestedTargetPercent = 0;
        row.appliedTargetPercent = 0;
        fields = splitCsvLine(MOTOR_OBSERVE::CSV::formatRow(row));
        CHECK(fields[6] == "Fault");
        CHECK(fields[10] == "LOW_LOW");

        row.physicalBackendAvailable = false;
        fields = splitCsvLine(MOTOR_OBSERVE::CSV::formatRow(row));
        CHECK(fields[10] == "NOT_APPLICABLE");
    }

    void testBrakeStopCsvState()
    {
        SafetyController safetyController;
        NoopBackend backend;
        BackendController controller(safetyController, backend);

        CHECK(controller.begin());
        CHECK(controller.prepareOutput());
        CHECK(controller.enableOutput());
        controller.setTargetPercent(40);
        controller.update();
        CHECK(controller.getState() == SafetyState::OutputEnabled);
        CHECK(controller.isPhysicalOutputAllowed());
        CHECK(controller.getLastAppliedTargetPercent() == 40);

        int requestedTargetPercent = 0;
        controller.setTargetPercent(0);
        controller.update();
        controller.disableOutput();
        controller.update();

        MOTOR_OBSERVE::CSV::Row brakeRow;
        brakeRow.sessionId = "MO-001";
        brakeRow.sampleIndex = 1;
        brakeRow.elapsedMs = 100;
        brakeRow.event = "target_change";
        brakeRow.safetyState = controller.getState();
        brakeRow.physicalOutputAllowed = controller.isPhysicalOutputAllowed();
        brakeRow.requestedTargetPercent = requestedTargetPercent;
        brakeRow.appliedTargetPercent = controller.getLastAppliedTargetPercent();
        brakeRow.measurementPathCode = "unknown";
        brakeRow.voltageSemantics = "unknown";
        brakeRow.currentSemantics = "unknown";
        brakeRow.powerSemantics = "unknown";
        brakeRow.note = "brake_stop";
        brakeRow.physicalBackendAvailable = true;

        std::vector<std::string> fields = splitCsvLine(MOTOR_OBSERVE::CSV::formatRow(brakeRow));
        CHECK(fields[6] == "SafeDisabled");
        CHECK(fields[7] == "0");
        CHECK(fields[8] == "0");
        CHECK(fields[9] == "0");
        CHECK(fields[10] == "LOW_LOW");
        CHECK(!fields[13].empty());
    }

    void testStopRowCsvState()
    {
        MOTOR_OBSERVE::CSV::Row stopRow;
        stopRow.sessionId = "MO-002";
        stopRow.sampleIndex = 2;
        stopRow.elapsedMs = 200;
        stopRow.event = "stop";
        stopRow.safetyState = SafetyState::SafeDisabled;
        stopRow.physicalOutputAllowed = false;
        stopRow.requestedTargetPercent = 0;
        stopRow.appliedTargetPercent = 0;
        stopRow.measurementPathCode = "unknown";
        stopRow.voltageSemantics = "unknown";
        stopRow.currentSemantics = "unknown";
        stopRow.powerSemantics = "unknown";
        stopRow.note = "stop";
        stopRow.physicalBackendAvailable = true;

        std::vector<std::string> fields = splitCsvLine(MOTOR_OBSERVE::CSV::formatRow(stopRow));
        CHECK(fields[5] == "stop");
        CHECK(fields[7] == "0");
        CHECK(fields[8] == "0");
        CHECK(fields[9] == "0");
        CHECK(fields[10] == "LOW_LOW");
    }

    void testLocalCsvDownloadRecordNameAllowlist()
    {
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-000.csv"));
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("REC-1000.csv"));
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("MO-000.csv"));
        CHECK(LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("MO-001.csv"));

        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("/spiflash/rec/MO-000.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("../MO-000.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("../../etc/passwd"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("MO-000.csv?x=1"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("MO-abc.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("MO-000.txt"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("OTHER-000.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("MO-00/0.csv"));
        CHECK(!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName("MO-00\\0.csv"));
    }

    void testFileLoggerWritesStartAndStopRows()
    {
        char originalDir[PATH_MAX];
        CHECK(getcwd(originalDir, sizeof(originalDir)) != nullptr);

        char tempDir[] = "/tmp/vameter_mo_csv_test_XXXXXX";
        CHECK(mkdtemp(tempDir) != nullptr);
        CHECK(chdir(tempDir) == 0);

        MOTOR_OBSERVE::CSV::FileLogger logger;
        MOTOR_OBSERVE::CSV::Row startRow;
        startRow.event = "start";
        startRow.safetyState = SafetyState::SafeDisabled;
        startRow.measurementPathCode = "unknown";
        startRow.voltageSemantics = "unknown";
        startRow.currentSemantics = "unknown";
        startRow.powerSemantics = "unknown";
        startRow.physicalBackendAvailable = true;

        CHECK(logger.begin(startRow));
        CHECK(logger.isOpen());
        CHECK(logger.fileName() == "MO-000.csv");
        CHECK(logger.append(startRow));

        MOTOR_OBSERVE::CSV::Row stopRow = startRow;
        stopRow.event = "stop";
        CHECK(logger.stop(stopRow));
        CHECK(!logger.isOpen());

        struct stat fileStat;
        CHECK(stat("MO-000.csv", &fileStat) == 0);
        CHECK(fileStat.st_size > 0);

        const std::string fileContent = readWholeFile("MO-000.csv");
        CHECK(fileContent.find(MOTOR_OBSERVE::CSV::headerLine()) != std::string::npos);
        CHECK(fileContent.find(",start,SafeDisabled,") != std::string::npos);
        CHECK(fileContent.find(",stop,SafeDisabled,") != std::string::npos);

        CHECK(chdir(originalDir) == 0);
        CHECK(std::remove(std::string(std::string(tempDir) + "/MO-000.csv").c_str()) == 0);
        CHECK(rmdir(tempDir) == 0);
    }

    void testClosedRecordFileDownloadReadiness()
    {
        char nonEmptyPath[] = "/tmp/vameter_mo_nonempty_XXXXXX";
        int nonEmptyFd = mkstemp(nonEmptyPath);
        CHECK(nonEmptyFd >= 0);
        CHECK(write(nonEmptyFd, "x", 1) == 1);
        CHECK(close(nonEmptyFd) == 0);

        char emptyPath[] = "/tmp/vameter_mo_empty_XXXXXX";
        int emptyFd = mkstemp(emptyPath);
        CHECK(emptyFd >= 0);
        CHECK(close(emptyFd) == 0);

        CHECK(LOCAL_CSV_DOWNLOAD::CheckClosedRecordFileForDownload("MO-000.csv", nonEmptyPath) ==
              LOCAL_CSV_DOWNLOAD::RecordFileStatus::Ready);
        CHECK(LOCAL_CSV_DOWNLOAD::CheckClosedRecordFileForDownload("MO-000.csv", emptyPath) ==
              LOCAL_CSV_DOWNLOAD::RecordFileStatus::Empty);
        CHECK(LOCAL_CSV_DOWNLOAD::CheckClosedRecordFileForDownload("../MO-000.csv", nonEmptyPath) ==
              LOCAL_CSV_DOWNLOAD::RecordFileStatus::InvalidName);
        CHECK(LOCAL_CSV_DOWNLOAD::CheckClosedRecordFileForDownload("MO-000.csv", "/tmp/vameter_missing_mo_file.csv") ==
              LOCAL_CSV_DOWNLOAD::RecordFileStatus::StatFailed);

        CHECK(std::remove(nonEmptyPath) == 0);
        CHECK(std::remove(emptyPath) == 0);
    }
} // namespace

int main()
{
    testSafetyController();
    testBackendControllerWithNoopBackend();
    testBackendControllerWithFaultBackend();
    testCsvHeaderAndRows();
    testBrakeStopCsvState();
    testStopRowCsvState();
    testLocalCsvDownloadRecordNameAllowlist();
    testFileLoggerWritesStartAndStopRows();
    testClosedRecordFileDownloadReadiness();

    if (g_failureCount != 0)
    {
        std::cerr << "motor_observe_state_test: fail (" << g_failureCount << " failure(s))" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "motor_observe_state_test: pass" << std::endl;
    return EXIT_SUCCESS;
}
