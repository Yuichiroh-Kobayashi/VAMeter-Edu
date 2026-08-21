#include "web_server_owner.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace
{
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }
} // namespace

int main()
{
    using WEB_SERVER_OWNER::Owner;
    using WEB_SERVER_OWNER::State;

    State state;
    Expect(state.owner() == Owner::None, "owner starts empty");
    Expect(state.generation() == 0, "generation starts at zero");
    Expect(!state.acquire(Owner::None), "cannot acquire the empty owner");

    Expect(state.acquire(Owner::System), "system owner acquires an empty server");
    Expect(state.owner() == Owner::System, "system owner is retained");
    Expect(state.generation() == 1, "successful acquire advances generation");
    Expect(!state.acquire(Owner::System), "duplicate system acquire is rejected");
    Expect(!state.acquire(Owner::Download), "download cannot displace system");
    Expect(!state.release(Owner::Download), "wrong owner cannot release system");
    Expect(state.owner() == Owner::System, "failed release preserves owner");
    Expect(state.release(Owner::System), "system owner releases server");
    Expect(state.owner() == Owner::None, "release clears owner");
    Expect(state.generation() == 1, "release retains generation for correlation");

    Expect(state.acquire(Owner::Download), "download acquires released server");
    Expect(state.generation() == 2, "next successful acquire advances generation");
    Expect(!state.acquire(Owner::System), "system cannot displace download");
    Expect(state.release(Owner::Download), "download releases server");
    Expect(!state.release(Owner::Download), "duplicate release is rejected");

    State wrapState(UINT32_MAX);
    Expect(wrapState.acquire(Owner::System), "wrap test acquire succeeds");
    Expect(wrapState.generation() == 1, "generation zero wrap is skipped");
    Expect(wrapState.release(Owner::System), "wrap test releases");
    Expect(!wrapState.acquire(Owner::None), "failed acquire does not advance generation");
    Expect(wrapState.generation() == 1, "failed acquire preserves generation");

    std::cout << "PASS: web server owner state\n";
    return 0;
}
