#include "runtime_evidence.h"

namespace RUNTIME_EVIDENCE
{
    namespace
    {
        const char* TokenForEvent(Event event)
        {
            switch (event)
            {
            case Event::Boot:
                return "boot";
            case Event::ServerRequest:
                return "server_request";
            case Event::ServerStart:
                return "server_start";
            case Event::ServerStop:
                return "server_stop";
            case Event::WebSocketConnect:
                return "websocket_connect";
            case Event::WebSocketDisconnect:
                return "websocket_disconnect";
            case Event::StreamStart:
                return "stream_start";
            case Event::StreamStop:
                return "stream_stop";
            case Event::StreamAbrupt:
                return "stream_abrupt";
            case Event::QueueSnapshot:
                return "queue_snapshot";
            case Event::SendFailure:
                return "send_failure";
            case Event::Pump:
                return "pump";
            case Event::HeapTrend:
                return "heap_trend";
            case Event::StackTrend:
                return "stack_trend";
            case Event::Unknown:
            default:
                return "unknown";
            }
        }

        const char* TokenForReason(Reason reason)
        {
            switch (reason)
            {
            case Reason::Boot:
                return "boot";
            case Reason::NetworkSettingsStart:
                return "network_settings_start";
            case Reason::NetworkSettingsIntentionalStop:
                return "network_settings_intentional_stop";
            case Reason::DownloadStart:
                return "download_start";
            case Reason::DownloadIntentionalStop:
                return "download_intentional_stop";
            case Reason::OwnerAcquire:
                return "owner_acquire";
            case Reason::OwnerRelease:
                return "owner_release";
            case Reason::WebSocketAccepted:
                return "websocket_accepted";
            case Reason::WebSocketRejected:
                return "websocket_rejected";
            case Reason::StreamAccepted:
                return "stream_accepted";
            case Reason::StreamOrderlyStopAccepted:
                return "stream_orderly_stop_accepted";
            case Reason::StreamOrderlyStopCompleted:
                return "stream_orderly_stop_completed";
            case Reason::Disconnect:
                return "disconnect";
            case Reason::ServerStop:
                return "server_stop";
            case Reason::SendFailure:
                return "send_failure";
            case Reason::QueueDrop:
                return "queue_drop";
            case Reason::ProducerDrop:
                return "producer_drop";
            case Reason::OutputDrop:
                return "output_drop";
            case Reason::InternalFailure:
                return "internal_failure";
            case Reason::OriginRejected:
                return "origin_rejected";
            case Reason::ProtocolViolation:
                return "protocol_violation";
            case Reason::ServerStartFailed:
                return "server_start_failed";
            case Reason::ServerStopFailed:
                return "server_stop_failed";
            case Reason::ActiveStreamTrend:
                return "active_stream_trend";
            case Reason::PumpScheduleAccepted:
                return "pump_schedule_accepted";
            case Reason::PumpScheduleCoalesced:
                return "pump_schedule_coalesced";
            case Reason::PumpQueueRejected:
                return "pump_queue_rejected";
            case Reason::PumpCallbackBegin:
                return "pump_callback_begin";
            case Reason::PumpCallbackEnd:
                return "pump_callback_end";
            case Reason::PumpStale:
                return "pump_stale";
            case Reason::Unknown:
            default:
                return "unknown";
            }
        }

        const char* TokenForResult(Result result)
        {
            switch (result)
            {
            case Result::Requested:
                return "requested";
            case Result::Succeeded:
                return "succeeded";
            case Result::Failed:
                return "failed";
            case Result::Rejected:
                return "rejected";
            case Result::Accepted:
                return "accepted";
            case Result::Completed:
                return "completed";
            case Result::Abrupt:
                return "abrupt";
            case Result::Dropped:
                return "dropped";
            case Result::Sent:
                return "sent";
            case Result::Observed:
                return "observed";
            case Result::Unknown:
            default:
                return "unknown";
            }
        }

        const char* TokenForOwner(Owner owner)
        {
            switch (owner)
            {
            case Owner::System:
                return "system";
            case Owner::Download:
                return "download";
            case Owner::None:
            default:
                return "none";
            }
        }

        class Appender
        {
        public:
            Appender(char* buffer, std::size_t capacity) : _buffer(buffer), _capacity(capacity), _length(0) {}

            void text(const char* value)
            {
                if (value == nullptr)
                    value = "";
                while (*value != '\0')
                {
                    put(*value);
                    ++value;
                }
            }

            void unsignedValue(std::uint64_t value)
            {
                char digits[20];
                std::size_t count = 0;
                do
                {
                    digits[count++] = static_cast<char>('0' + value % 10U);
                    value /= 10U;
                } while (value != 0);
                while (count != 0)
                    put(digits[--count]);
            }

            void signedValue(std::int32_t value)
            {
                if (value < 0)
                {
                    put('-');
                    const std::int64_t magnitude = -static_cast<std::int64_t>(value);
                    unsignedValue(static_cast<std::uint64_t>(magnitude));
                }
                else
                {
                    unsignedValue(static_cast<std::uint64_t>(value));
                }
            }

            std::size_t length() const { return _length; }

            void terminate()
            {
                if (_buffer != nullptr && _capacity != 0)
                {
                    const std::size_t end = _length < _capacity ? _length : _capacity - 1;
                    _buffer[end] = '\0';
                }
            }

        private:
            void put(char value)
            {
                if (_buffer != nullptr && _capacity > 1 && _length < _capacity - 1)
                    _buffer[_length] = value;
                ++_length;
            }

            char* _buffer;
            std::size_t _capacity;
            std::size_t _length;
        };

        void AppendResource(Appender& output, const Resource& resource)
        {
            output.text("D2B_DIAG event=");
            output.text(TokenForEvent(resource.event));
            output.text(" reason=");
            output.text(TokenForReason(resource.reason));
            output.text(" result=");
            output.text(TokenForResult(resource.result));
            output.text(" owner=");
            output.text(TokenForOwner(resource.owner));
            output.text(" generation=");
            output.unsignedValue(resource.generation);
            output.text(" server_generation=");
            output.unsignedValue(resource.server_generation);
            output.text(" socket=");
            output.signedValue(resource.socket);
            output.text(" stream_id=");
            output.unsignedValue(resource.stream_id);
            output.text(" heap_free=");
            output.unsignedValue(resource.heap_free);
            output.text(" heap_min=");
            output.unsignedValue(resource.heap_min);
            output.text(" heap_largest=");
            output.unsignedValue(resource.heap_largest);
            output.text(" encoder_stack_min=");
            output.unsignedValue(resource.encoder_stack_min);
            output.text(" tx_stack_min=");
            output.unsignedValue(resource.tx_stack_min);
            output.text(" acquisition_depth=");
            output.unsignedValue(resource.acquisition_depth);
            output.text(" output_depth=");
            output.unsignedValue(resource.output_depth);
            output.text(" producer_drops=");
            output.unsignedValue(resource.producer_drops);
            output.text(" output_drops=");
            output.unsignedValue(resource.output_drops);
        }
    } // namespace

    const char* EventToken(Event event) { return TokenForEvent(event); }
    const char* ReasonToken(Reason reason) { return TokenForReason(reason); }
    const char* ResultToken(Result result) { return TokenForResult(result); }
    const char* OwnerToken(Owner owner) { return TokenForOwner(owner); }

    std::size_t FormatLine(const Resource& resource, char* buffer, std::size_t capacity)
    {
        Appender output(buffer, capacity);
        AppendResource(output, resource);
        output.terminate();
        return output.length();
    }

    std::size_t FormatBootLine(const BootResource& resource, char* buffer, std::size_t capacity)
    {
        Appender output(buffer, capacity);
        AppendResource(output, resource.resource);
        output.text(" reset_reason_code=");
        output.unsignedValue(resource.reset_reason_code);
        output.text(" reset_reason_raw=");
        output.unsignedValue(resource.reset_reason_raw);
        output.text(" boot_identity=");
        output.unsignedValue(resource.boot_identity);
        output.text(" rtc_boot_counter=");
        output.unsignedValue(resource.rtc_boot_counter);
        output.text(" prior_valid=");
        output.unsignedValue(resource.prior_valid);
        output.text(" prior_stage=");
        output.unsignedValue(resource.prior_stage);
        output.text(" prior_sequence=");
        output.unsignedValue(resource.prior_sequence);
        output.text(" prior_server_generation=");
        output.unsignedValue(resource.prior_server_generation);
        output.text(" prior_websocket_generation=");
        output.unsignedValue(resource.prior_websocket_generation);
        output.text(" prior_socket=");
        output.signedValue(resource.prior_socket);
        output.text(" prior_stream_id=");
        output.unsignedValue(resource.prior_stream_id);
        output.text(" prior_configured_httpd_stack_bytes=");
        output.unsignedValue(resource.prior_configured_httpd_stack_bytes);
        output.text(" prior_httpd_stack_sample_valid=");
        output.unsignedValue(resource.prior_httpd_stack_sample_valid);
        output.text(" prior_httpd_stack_high_water_raw=");
        output.unsignedValue(resource.prior_httpd_stack_high_water_raw);
        output.text(" prior_httpd_stack_high_water_bytes=");
        output.unsignedValue(resource.prior_httpd_stack_high_water_bytes);
        output.text(" prior_internal_heap_free=");
        output.unsignedValue(resource.prior_internal_heap_free);
        output.text(" prior_internal_heap_min=");
        output.unsignedValue(resource.prior_internal_heap_min);
        output.text(" prior_internal_heap_largest=");
        output.unsignedValue(resource.prior_internal_heap_largest);
        output.text(" prior_reset_reason_code=");
        output.unsignedValue(resource.prior_reset_reason_code);
        output.text(" prior_reset_reason_raw=");
        output.unsignedValue(resource.prior_reset_reason_raw);
        output.text(" prior_checksum=");
        output.unsignedValue(resource.prior_checksum);
        output.terminate();
        return output.length();
    }

    RateLimiter::RateLimiter(std::uint64_t intervalUs)
        : _intervalUs(intervalUs < kMinimumRateIntervalUs ? kMinimumRateIntervalUs : intervalUs), _lastUs(0), _hasSample(false)
    {
    }

    bool RateLimiter::Allow(std::uint64_t nowUs)
    {
        if (!_hasSample || nowUs < _lastUs || nowUs - _lastUs >= _intervalUs)
        {
            _lastUs = nowUs;
            _hasSample = true;
            return true;
        }
        return false;
    }

    void RateLimiter::Reset()
    {
        _lastUs = 0;
        _hasSample = false;
    }
} // namespace RUNTIME_EVIDENCE
