#pragma once

#include <cstddef>
#include <cstdint>

namespace RUNTIME_EVIDENCE
{
    static const std::uint64_t kMinimumRateIntervalUs = 5000000ULL;

    enum class Event : std::uint8_t
    {
        Unknown,
        Boot,
        ServerRequest,
        ServerStart,
        ServerStop,
        WebSocketConnect,
        WebSocketDisconnect,
        StreamStart,
        StreamStop,
        StreamAbrupt,
        QueueSnapshot,
        SendFailure,
        HeapTrend,
        StackTrend,
    };

    enum class Reason : std::uint8_t
    {
        Unknown,
        Boot,
        NetworkSettingsStart,
        NetworkSettingsIntentionalStop,
        DownloadStart,
        DownloadIntentionalStop,
        OwnerAcquire,
        OwnerRelease,
        WebSocketAccepted,
        WebSocketRejected,
        StreamAccepted,
        StreamOrderlyStopAccepted,
        StreamOrderlyStopCompleted,
        Disconnect,
        ServerStop,
        SendFailure,
        QueueDrop,
        ProducerDrop,
        OutputDrop,
        InternalFailure,
        OriginRejected,
        ProtocolViolation,
        ServerStartFailed,
        ServerStopFailed,
        ActiveStreamTrend,
    };

    enum class Result : std::uint8_t
    {
        Unknown,
        Requested,
        Succeeded,
        Failed,
        Rejected,
        Accepted,
        Completed,
        Abrupt,
        Dropped,
        Sent,
        Observed,
    };

    enum class Owner : std::uint8_t
    {
        None,
        System,
        Download,
    };

    struct Resource
    {
        Event event;
        Reason reason;
        Result result;
        Owner owner;
        std::uint32_t generation;
        std::uint32_t server_generation;
        std::int32_t socket;
        std::uint32_t stream_id;
        std::uint32_t heap_free;
        std::uint32_t heap_min;
        std::uint32_t heap_largest;
        std::uint32_t encoder_stack_min;
        std::uint32_t tx_stack_min;
        std::uint32_t acquisition_depth;
        std::uint32_t output_depth;
        std::uint64_t producer_drops;
        std::uint64_t output_drops;
    };

    struct BootResource
    {
        Resource resource;
        std::uint32_t reset_reason_code;
        std::uint32_t reset_reason_raw;
        std::uint32_t boot_identity;
        std::uint32_t rtc_boot_counter;
    };

    const char* EventToken(Event event);
    const char* ReasonToken(Reason reason);
    const char* ResultToken(Result result);
    const char* OwnerToken(Owner owner);

    /*
     * FormatLine and FormatBootLine use a caller-owned buffer and do not
     * allocate.  They return the complete line length excluding the newline
     * and terminating NUL.  On truncation the returned length is still the
     * required length, while the buffer is always NUL terminated when its
     * capacity is non-zero.
     */
    std::size_t FormatLine(const Resource& resource, char* buffer, std::size_t capacity);
    std::size_t FormatBootLine(const BootResource& resource, char* buffer, std::size_t capacity);

    class RateLimiter
    {
    public:
        explicit RateLimiter(std::uint64_t intervalUs = kMinimumRateIntervalUs);

        bool Allow(std::uint64_t nowUs);
        void Reset();
        bool hasSample() const { return _hasSample; }
        std::uint64_t lastUs() const { return _lastUs; }
        std::uint64_t intervalUs() const { return _intervalUs; }

    private:
        std::uint64_t _intervalUs;
        std::uint64_t _lastUs;
        bool _hasSample;
    };
} // namespace RUNTIME_EVIDENCE
