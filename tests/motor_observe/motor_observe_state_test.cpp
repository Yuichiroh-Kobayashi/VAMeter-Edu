/*
 * SPDX-License-Identifier: MIT
 */
#include "libs/motor_observe_backend/motor_observe_backend.h"

#include <cstdlib>
#include <iostream>
#include <string>

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
} // namespace

int main()
{
    testSafetyController();
    testBackendControllerWithNoopBackend();
    testBackendControllerWithFaultBackend();

    if (g_failureCount != 0)
    {
        std::cerr << "motor_observe_state_test: fail (" << g_failureCount << " failure(s))" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "motor_observe_state_test: pass" << std::endl;
    return EXIT_SUCCESS;
}
