#pragma once

namespace RELAY_INTERLOCK
{
    struct Command
    {
        bool apply;
        bool state;
    };

    class RelayInterlock
    {
    public:
        RelayInterlock();
        void initialize();
        Command requestNormal(bool state) const;
        Command commitFault();
        bool isInitialized() const;
        bool isFaultLatched() const;

    private:
        bool _initialized;
        bool _faultLatched;
    };
} // namespace RELAY_INTERLOCK
