#pragma once

#include "../../../libs/live_share_session/live_share_session.h"

#include <cstdint>
#include <string>
#include <vector>

namespace VIEWS
{
    class LiveShareView
    {
    public:
        LiveShareView();

        void update(LIVE_SHARE_SESSION::State state,
                    std::uint32_t themeColor,
                    const std::string& activeApSsid,
                    const std::string& trustedViewerUrl,
                    LIVE_SHARE_SESSION::StartOutcome startOutcome);

    private:
        void prepareQr(LIVE_SHARE_SESSION::State state,
                       const std::string& activeApSsid,
                       const std::string& trustedViewerUrl);
        void render(LIVE_SHARE_SESSION::State state,
                    std::uint32_t themeColor,
                    LIVE_SHARE_SESSION::StartOutcome startOutcome);

        LIVE_SHARE_SESSION::State _preparedState;
        std::string _preparedPayload;
        std::vector<std::vector<bool>> _qrBitmap;
    };
} // namespace VIEWS
