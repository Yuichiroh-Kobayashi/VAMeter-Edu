#pragma once

#include <atomic>
#include <cstdint>

namespace WEB_SERVER_PROFILE
{
    class AdmissionReadiness
    {
    public:
        AdmissionReadiness() : _ready(false) {}

        bool isReady() const { return _ready.load(std::memory_order_acquire); }

        bool allowsSessionContextCreation() const { return isReady(); }

        void setReady() { _ready.store(true, std::memory_order_release); }

        void reset() { _ready.store(false, std::memory_order_release); }

    private:
        std::atomic<bool> _ready;
    };

    enum class Profile : std::uint8_t
    {
        SystemConfig,
        SystemLive,
    };

    struct RoutePolicy
    {
        bool configurationPages;
        bool configurationApis;
        bool viewerRequired;
        bool d2bRequired;
        bool originRxRequired;
        bool downloadRoutesAllowed;
    };

    inline RoutePolicy PolicyFor(Profile profile)
    {
        if (profile == Profile::SystemLive)
        {
            const RoutePolicy policy = {
                false,
                false,
                true,
                true,
                true,
                false,
            };
            return policy;
        }

        const RoutePolicy policy = {
            true,
            true,
            false,
            false,
            false,
            false,
        };
        return policy;
    }
} // namespace WEB_SERVER_PROFILE
