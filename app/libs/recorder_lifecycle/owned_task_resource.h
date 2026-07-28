/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <memory>
#include <utility>

namespace RECORDER_LIFECYCLE
{
    template <typename Resource>
    class OwnedTaskResource
    {
    public:
        enum Phase
        {
            phase_empty = 0,
            phase_preparing,
            phase_running,
            phase_finished,
        };

    private:
        std::unique_ptr<Resource> _resource;
        Phase _phase = phase_empty;
        bool _stop_requested = false;

    public:
        bool acquire(std::unique_ptr<Resource> resource)
        {
            if (!resource || _resource)
                return false;

            _resource = std::move(resource);
            _phase = phase_preparing;
            _stop_requested = false;
            return true;
        }

        void taskStarted()
        {
            if (_resource)
                _phase = phase_running;
        }

        void taskCreateFailed()
        {
            _resource.reset();
            _phase = phase_empty;
            _stop_requested = false;
        }

        void requestStop()
        {
            if (_resource)
                _stop_requested = true;
        }

        void taskFinished()
        {
            if (_resource)
                _phase = phase_finished;
        }

        bool releaseFinished()
        {
            if (!_resource)
                return true;
            if (_phase != phase_finished)
                return false;

            _resource.reset();
            _phase = phase_empty;
            _stop_requested = false;
            return true;
        }

        Resource* get() const { return _resource.get(); }
        bool hasResource() const { return static_cast<bool>(_resource); }
        bool isActive() const { return _phase == phase_preparing || _phase == phase_running; }
        bool isFinished() const { return _phase == phase_finished; }
        bool stopRequested() const { return _stop_requested; }
        Phase phase() const { return _phase; }
    };
} // namespace RECORDER_LIFECYCLE
