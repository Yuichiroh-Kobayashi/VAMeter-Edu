/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <string>

#include "../motor_observe_safety/motor_observe_safety.h"

namespace MOTOR_OBSERVE
{
    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual bool begin() = 0;
        virtual void disarm() = 0;
        virtual void setTargetPercent(int targetPercent) = 0;
        virtual void update() = 0;
        virtual bool hasFault() const = 0;
        virtual const std::string& getFaultReason() const = 0;
        virtual int getLastAppliedTargetPercent() const = 0;
    };

    class NoopBackend : public Backend
    {
    public:
        bool begin() override;
        void disarm() override;
        void setTargetPercent(int targetPercent) override;
        void update() override;
        bool hasFault() const override;
        const std::string& getFaultReason() const override;
        int getLastAppliedTargetPercent() const override;

    private:
        int _lastAppliedTargetPercent = 0;
        std::string _faultReason;
    };

    class BackendController
    {
    public:
        BackendController(SafetyController& safetyController, Backend& backend);

        bool begin();
        bool prepareOutput();
        bool enableOutput();
        void disableOutput();
        void leaveMode();
        void timeout();
        void setFault(const std::string& reason);
        bool clearFault();
        void setTargetPercent(int targetPercent);
        void update();

        SafetyState getState() const;
        int getTargetPercent() const;
        int getLastAppliedTargetPercent() const;
        bool isPhysicalOutputAllowed() const;
        bool hasFault() const;
        const std::string& getFaultReason() const;

    private:
        SafetyController& _safetyController;
        Backend& _backend;

        void _applyZeroTarget();
    };
} // namespace MOTOR_OBSERVE
