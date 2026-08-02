#pragma once

#include "../../../libs/web_server_owner/web_server_results.h"

namespace NETWORK_WEB_SERVER_RECOVERY
{
    enum class Action
    {
        ContinueWorkflow,
        ShowBusyNotice,
        EnterStopRecovery,
        ShowStartFailure,
    };

    enum class MenuAction
    {
        RetryStop,
        PowerCycleHelp,
    };

    Action DecideStartAction(WEB_SERVER_OWNER::StartResult result);
    bool StopCompleted(WEB_SERVER_OWNER::StopResult result);
    bool KeepRecovery(WEB_SERVER_OWNER::StopResult result);
    const char* StopResultMessage(WEB_SERVER_OWNER::StopResult result);
} // namespace NETWORK_WEB_SERVER_RECOVERY
