#include "signed_current_observation.h"

#include <cstdio>

namespace SIGNED_CURRENT_OBS
{
    const char* CurrentRangeToken(CurrentRange range)
    {
        switch (range)
        {
        case CurrentRange::Low:
            return "LC";
        case CurrentRange::High:
            return "HC";
        }
        return "UNKNOWN";
    }

    bool FormatObservation(const ObservationRecord& record,
                           std::uint64_t droppedRecordCount,
                           char* output,
                           std::size_t outputCapacity,
                           std::size_t& outputLength)
    {
        outputLength = 0;
        if (output == nullptr || outputCapacity == 0)
            return false;

        const int length = std::snprintf(output,
                                         outputCapacity,
                                         "SIGNED_CURRENT_OBS v=1 seq=%llu timestamp_us=%llu current_A=%+.9e bus_V=%+.9e "
                                         "valid_mask=0x%08lx range=%s current_read_ok=%u overflow_read_ok=%u "
                                         "overflow=%u dropped_total=%llu",
                                         static_cast<unsigned long long>(record.sequence),
                                         static_cast<unsigned long long>(record.timestampUs),
                                         static_cast<double>(record.shuntCurrentA),
                                         static_cast<double>(record.busVoltageV),
                                         static_cast<unsigned long>(record.validMask),
                                         CurrentRangeToken(record.currentRange),
                                         record.currentReadSucceeded ? 1U : 0U,
                                         record.overflowReadSucceeded ? 1U : 0U,
                                         record.overflowAsserted ? 1U : 0U,
                                         static_cast<unsigned long long>(droppedRecordCount));
        if (length < 0 || static_cast<std::size_t>(length) >= outputCapacity)
        {
            output[outputCapacity - 1] = '\0';
            return false;
        }

        outputLength = static_cast<std::size_t>(length);
        return true;
    }
} // namespace SIGNED_CURRENT_OBS
