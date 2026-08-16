#include "web_server_profile.h"

#include <cstdlib>
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
    WEB_SERVER_PROFILE::AdmissionReadiness readiness;
    Expect(!readiness.isReady(), "new server lifecycle starts admission-not-ready");
    Expect(!readiness.allowsSessionContextCreation(), "pre-ready open cannot create scanner context");

    readiness.setReady();
    Expect(readiness.isReady(), "future complete SystemLive can arm admission");
    Expect(readiness.allowsSessionContextCreation(), "ready lifecycle can create scanner context");

    readiness.reset();
    Expect(!readiness.isReady(), "ready-to-not-ready transition blocks new admission");
    Expect(!readiness.allowsSessionContextCreation(), "reset blocks scanner context creation");

    WEB_SERVER_PROFILE::AdmissionReadiness nextLifecycle;
    Expect(!nextLifecycle.isReady(), "next server lifecycle starts admission-not-ready");

    using WEB_SERVER_PROFILE::PolicyFor;
    using WEB_SERVER_PROFILE::Profile;

    const WEB_SERVER_PROFILE::RoutePolicy config = PolicyFor(Profile::SystemConfig);
    Expect(config.configurationPages, "SystemConfig allows configuration pages");
    Expect(config.configurationApis, "SystemConfig allows configuration APIs");
    Expect(!config.viewerRequired, "SystemConfig forbids Viewer routes");
    Expect(!config.d2bRequired, "SystemConfig forbids D2B routes");
    Expect(!config.originRxRequired, "SystemConfig does not install O1-RX");
    Expect(!config.downloadRoutesAllowed, "SystemConfig forbids Download routes");

    const WEB_SERVER_PROFILE::RoutePolicy live = PolicyFor(Profile::SystemLive);
    Expect(!live.configurationPages, "SystemLive forbids configuration pages");
    Expect(!live.configurationApis, "SystemLive forbids configuration APIs");
    Expect(live.viewerRequired, "SystemLive requires Viewer routes");
    Expect(live.d2bRequired, "SystemLive requires D2B routes");
    Expect(live.originRxRequired, "SystemLive requires O1-RX");
    Expect(!live.downloadRoutesAllowed, "SystemLive forbids Download routes");

    std::cout << "PASS: stable SystemConfig/SystemLive route policy\n";
    return 0;
}
