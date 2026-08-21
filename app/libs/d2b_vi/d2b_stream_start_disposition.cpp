#include "d2b_stream_start_disposition.h"

namespace D2B
{
    StreamStartDisposition DecideStreamStartDisposition(bool pipelineStartSucceeded)
    {
        return pipelineStartSucceeded ? StreamStartDisposition::ContinueConnection
                                      : StreamStartDisposition::CloseConnection;
    }
} // namespace D2B
