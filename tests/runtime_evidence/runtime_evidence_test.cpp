#include "runtime_evidence.h"

#include <cstdlib>
#include <cstring>
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

    bool IsMachineToken(const char* token)
    {
        if (token == nullptr || *token == '\0')
            return false;
        for (const char* cursor = token; *cursor != '\0'; ++cursor)
        {
            const char value = *cursor;
            if (!((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '_'))
                return false;
        }
        return true;
    }

    RUNTIME_EVIDENCE::Resource Sample()
    {
        RUNTIME_EVIDENCE::Resource resource = {};
        resource.event = RUNTIME_EVIDENCE::Event::QueueSnapshot;
        resource.reason = RUNTIME_EVIDENCE::Reason::OutputDrop;
        resource.result = RUNTIME_EVIDENCE::Result::Observed;
        resource.owner = RUNTIME_EVIDENCE::Owner::System;
        resource.generation = 7;
        resource.server_generation = 17;
        resource.socket = -1;
        resource.stream_id = 11;
        resource.heap_free = 101;
        resource.heap_min = 102;
        resource.heap_largest = 103;
        resource.encoder_stack_min = 104;
        resource.tx_stack_min = 105;
        resource.acquisition_depth = 6;
        resource.output_depth = 5;
        resource.producer_drops = 8;
        resource.output_drops = 9;
        return resource;
    }
} // namespace

int main()
{
    using namespace RUNTIME_EVIDENCE;

    const Event events[] = {
        Event::Unknown,
        Event::Boot,
        Event::ServerRequest,
        Event::ServerStart,
        Event::ServerStop,
        Event::WebSocketConnect,
        Event::WebSocketDisconnect,
        Event::StreamStart,
        Event::StreamStop,
        Event::StreamAbrupt,
        Event::QueueSnapshot,
        Event::SendFailure,
        Event::HeapTrend,
        Event::StackTrend,
    };
    for (const Event event : events)
        Expect(IsMachineToken(EventToken(event)), "event token is machine-safe");
    Expect(std::strcmp(EventToken(Event::ServerStart), "server_start") == 0, "server start event token");
    Expect(std::strcmp(EventToken(Event::ServerStop), "server_stop") == 0, "server stop event token");
    Expect(std::strcmp(EventToken(Event::SendFailure), "send_failure") == 0, "send failure event token");

    const Reason reasons[] = {
        Reason::Unknown,
        Reason::Boot,
        Reason::NetworkSettingsStart,
        Reason::NetworkSettingsIntentionalStop,
        Reason::DownloadStart,
        Reason::DownloadIntentionalStop,
        Reason::OwnerAcquire,
        Reason::OwnerRelease,
        Reason::WebSocketAccepted,
        Reason::WebSocketRejected,
        Reason::StreamAccepted,
        Reason::StreamOrderlyStopAccepted,
        Reason::StreamOrderlyStopCompleted,
        Reason::Disconnect,
        Reason::ServerStop,
        Reason::SendFailure,
        Reason::QueueDrop,
        Reason::ProducerDrop,
        Reason::OutputDrop,
        Reason::InternalFailure,
        Reason::OriginRejected,
        Reason::ProtocolViolation,
        Reason::ServerStartFailed,
        Reason::ServerStopFailed,
        Reason::ActiveStreamTrend,
    };
    for (const Reason reason : reasons)
        Expect(IsMachineToken(ReasonToken(reason)), "reason token is machine-safe");
    Expect(std::strcmp(ReasonToken(Reason::NetworkSettingsStart), "network_settings_start") == 0,
           "network settings start reason token");
    Expect(std::strcmp(ReasonToken(Reason::NetworkSettingsIntentionalStop),
                       "network_settings_intentional_stop") == 0,
           "network settings intentional stop reason token");
    Expect(std::strcmp(ReasonToken(Reason::DownloadStart), "download_start") == 0,
           "download start reason token");
    Expect(std::strcmp(ReasonToken(Reason::DownloadIntentionalStop), "download_intentional_stop") == 0,
           "download intentional stop reason token");
    Expect(std::strcmp(ReasonToken(Reason::ServerStop), "server_stop") == 0, "server stop reason token");
    Expect(std::strcmp(ReasonToken(Reason::SendFailure), "send_failure") == 0, "send failure reason token");

    const Result results[] = {Result::Unknown,
                              Result::Requested,
                              Result::Succeeded,
                              Result::Failed,
                              Result::Rejected,
                              Result::Accepted,
                              Result::Completed,
                              Result::Abrupt,
                              Result::Dropped,
                              Result::Sent,
                              Result::Observed};
    for (const Result result : results)
        Expect(IsMachineToken(ResultToken(result)), "result token is machine-safe");
    Expect(IsMachineToken(OwnerToken(Owner::None)), "none owner token is machine-safe");
    Expect(IsMachineToken(OwnerToken(Owner::System)), "system owner token is machine-safe");
    Expect(IsMachineToken(OwnerToken(Owner::Download)), "download owner token is machine-safe");
    Expect(std::strcmp(OwnerToken(Owner::None), "none") == 0, "none owner token");
    Expect(std::strcmp(OwnerToken(Owner::System), "system") == 0, "system owner token");
    Expect(std::strcmp(OwnerToken(Owner::Download), "download") == 0, "download owner token");
    Expect(std::strcmp(ResultToken(Result::Requested), "requested") == 0, "requested result token");
    Expect(std::strcmp(ResultToken(Result::Succeeded), "succeeded") == 0, "succeeded result token");
    Expect(std::strcmp(ResultToken(Result::Failed), "failed") == 0, "failed result token");

    char line[768];
    const Resource resource = Sample();
    const std::size_t length = FormatLine(resource, line, sizeof(line));
    Expect(length > 0 && length < sizeof(line), "line fits caller buffer");
    Expect(std::strncmp(line, "D2B_DIAG event=queue_snapshot reason=output_drop result=observed owner=system",
                        std::strlen("D2B_DIAG event=queue_snapshot reason=output_drop result=observed owner=system")) == 0,
           "fixed prefix and token order");
    Expect(std::strstr(line,
                       " generation=7 server_generation=17 socket=-1 stream_id=11 heap_free=101 heap_min=102 heap_largest=103") != nullptr,
           "resource fields and sentinel");
    Expect(std::strstr(line, " encoder_stack_min=104 tx_stack_min=105 acquisition_depth=6 output_depth=5 producer_drops=8 output_drops=9") != nullptr,
           "resource tail field order");

    char truncated[24];
    const std::size_t required = FormatLine(resource, truncated, sizeof(truncated));
    Expect(required == length, "truncation reports required length");
    Expect(truncated[sizeof(truncated) - 1] == '\0', "truncation remains terminated");
    Expect(std::strncmp(truncated, "D2B_DIAG event=queue_sna", sizeof(truncated) - 1) == 0,
           "truncation preserves prefix");

    BootResource boot = {};
    boot.resource = resource;
    boot.resource.event = Event::Boot;
    boot.resource.reason = Reason::Boot;
    boot.reset_reason_code = 3;
    boot.reset_reason_raw = 0x1234;
    boot.boot_identity = 55;
    boot.rtc_boot_counter = 8;
    const std::size_t bootLength = FormatBootLine(boot, line, sizeof(line));
    Expect(bootLength > length, "boot fields append to resource line");
    Expect(std::strstr(line, " generation=7 server_generation=17 socket=-1") != nullptr,
           "boot preserves fixed resource field order");
    Expect(std::strstr(line, " reset_reason_code=3 reset_reason_raw=4660 boot_identity=55 rtc_boot_counter=8") != nullptr,
           "boot identity fields");

    RateLimiter shortLimiter(1);
    Expect(shortLimiter.intervalUs() == kMinimumRateIntervalUs, "short custom interval clamps to five seconds");
    Expect(shortLimiter.Allow(100), "clamped limiter initial emission allowed");
    Expect(!shortLimiter.Allow(100 + kMinimumRateIntervalUs - 1), "clamped limiter rejects under five seconds");
    Expect(shortLimiter.Allow(100 + kMinimumRateIntervalUs), "clamped limiter accepts at five seconds");

    RateLimiter limiter;
    Expect(limiter.Allow(100), "initial emission allowed");
    Expect(!limiter.Allow(100 + kMinimumRateIntervalUs - 1), "under five seconds rejected");
    Expect(limiter.Allow(100 + kMinimumRateIntervalUs), "five seconds accepted");
    Expect(limiter.Allow(7), "clock rollback recovers");
    Expect(!limiter.Allow(7 + kMinimumRateIntervalUs - 1), "rollback interval gate");
    Expect(limiter.Allow(7 + kMinimumRateIntervalUs), "rollback gate accepts at interval");

    std::cout << "PASS: runtime evidence formatter and limiter\n";
    return 0;
}
