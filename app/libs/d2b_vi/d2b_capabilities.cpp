#include "d2b_capabilities.h"

namespace D2B
{
    const char* CapabilitiesJson()
    {
        static const char capabilities[] =
            "{\"protocol\":\"d2b-stream\",\"version\":\"0.1\",\"maximum_binary_frame_size\":48,"
            "\"maximum_control_message_size\":2048,\"maximum_active_stream_sessions\":1,"
            "\"maximum_control_connections\":1,\"persistent_capture_supported\":false,"
            "\"security_mode\":\"unauthenticated-read-only\",\"streams\":[{\"id\":\"live-vi\","
            "\"label\":\"Voltage and current\",\"profiles\":[{\"profile\":\"vi-measurement\","
            "\"parameter_sets\":[{\"sample_format\":\"vi-f32le\",\"channel_count\":2,\"channel_mask\":3,"
            "\"sample_rate\":{\"numerator\":0,\"denominator\":0}}]}]}]}";
        return capabilities;
    }
} // namespace D2B
