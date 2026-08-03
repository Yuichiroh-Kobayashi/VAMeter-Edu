#include "reset_breadcrumb.h"

#include <atomic>
#include <limits>

namespace D2B_RESET_BREADCRUMB
{
    namespace
    {
        struct StageToken
        {
            std::uint32_t id;
            const char* name;
        };

        const StageToken kStages[] = {
            {static_cast<std::uint32_t>(Stage::NONE), "NONE"},
            {static_cast<std::uint32_t>(Stage::BOOT_REPORTED), "BOOT_REPORTED"},
            {static_cast<std::uint32_t>(Stage::SERVER_STARTED), "SERVER_STARTED"},
            {static_cast<std::uint32_t>(Stage::WS_HANDLER_GET_AFTER_101_ENTER), "WS_HANDLER_GET_AFTER_101_ENTER"},
            {static_cast<std::uint32_t>(Stage::ORIGIN_CHECK_BEGIN), "ORIGIN_CHECK_BEGIN"},
            {static_cast<std::uint32_t>(Stage::ORIGIN_CHECK_OK), "ORIGIN_CHECK_OK"},
            {static_cast<std::uint32_t>(Stage::OWNER_PUBLISHED), "OWNER_PUBLISHED"},
            {static_cast<std::uint32_t>(Stage::PIPELINE_OPEN_ENTER), "PIPELINE_OPEN_ENTER"},
            {static_cast<std::uint32_t>(Stage::PIPELINE_OPEN_OK), "PIPELINE_OPEN_OK"},
            {static_cast<std::uint32_t>(Stage::CONNECT_EVIDENCE_ENTER), "CONNECT_EVIDENCE_ENTER"},
            {static_cast<std::uint32_t>(Stage::CONNECT_EVIDENCE_OK), "CONNECT_EVIDENCE_OK"},
            {static_cast<std::uint32_t>(Stage::OPEN_HANDLER_RETURN), "OPEN_HANDLER_RETURN"},
            {static_cast<std::uint32_t>(Stage::WS_FRAME_HANDLER_ENTER), "WS_FRAME_HANDLER_ENTER"},
            {static_cast<std::uint32_t>(Stage::FRAME_HEADER_READ_OK), "FRAME_HEADER_READ_OK"},
            {static_cast<std::uint32_t>(Stage::FRAME_PAYLOAD_READ_OK), "FRAME_PAYLOAD_READ_OK"},
            {static_cast<std::uint32_t>(Stage::REASSEMBLY_COMPLETE), "REASSEMBLY_COMPLETE"},
            {static_cast<std::uint32_t>(Stage::CONTROL_PARSE_ENTER), "CONTROL_PARSE_ENTER"},
            {static_cast<std::uint32_t>(Stage::CONTROL_PARSE_OK), "CONTROL_PARSE_OK"},
            {static_cast<std::uint32_t>(Stage::HELLO_RECOGNIZED), "HELLO_RECOGNIZED"},
            {static_cast<std::uint32_t>(Stage::CONTROL_HANDLE_ENTER), "CONTROL_HANDLE_ENTER"},
            {static_cast<std::uint32_t>(Stage::WELCOME_BUILT), "WELCOME_BUILT"},
            {static_cast<std::uint32_t>(Stage::WELCOME_SEND_ENTER), "WELCOME_SEND_ENTER"},
            {static_cast<std::uint32_t>(Stage::WELCOME_SEND_OK), "WELCOME_SEND_OK"},
            {static_cast<std::uint32_t>(Stage::SESSION_READY_COMMIT), "SESSION_READY_COMMIT"},
            {static_cast<std::uint32_t>(Stage::WS_RECEIVE_RETURN), "WS_RECEIVE_RETURN"},
            {static_cast<std::uint32_t>(Stage::WS_CLOSE_ENTER), "WS_CLOSE_ENTER"},
            {static_cast<std::uint32_t>(Stage::WS_CLOSE_COMPLETE), "WS_CLOSE_COMPLETE"},
            {static_cast<std::uint32_t>(Stage::NORMAL_LIFECYCLE_COMPLETE), "NORMAL_LIFECYCLE_COMPLETE"},
        };

        static const std::size_t kStageCount = sizeof(kStages) / sizeof(kStages[0]);

        std::uint32_t CrcByte(std::uint32_t crc, std::uint8_t value)
        {
            crc ^= value;
            for (unsigned bit = 0; bit < 8U; ++bit)
                crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
            return crc;
        }

        std::uint32_t CrcWord(std::uint32_t crc, std::uint32_t value)
        {
            crc = CrcByte(crc, static_cast<std::uint8_t>(value & 0xFFU));
            crc = CrcByte(crc, static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
            crc = CrcByte(crc, static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
            return CrcByte(crc, static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
        }

        std::uint32_t FinishCrc(std::uint32_t crc) { return crc ^ 0xFFFFFFFFU; }

        std::uint32_t StageValue(Stage stage) { return static_cast<std::uint32_t>(stage); }

        class Appender
        {
        public:
            Appender(char* buffer, std::size_t capacity) : _buffer(buffer), _capacity(capacity), _length(0U) {}

            void Text(const char* value)
            {
                if (value == nullptr)
                    value = "";
                while (*value != '\0')
                    Put(*value++);
            }

            void Unsigned(std::uint32_t value)
            {
                char digits[10];
                std::size_t count = 0U;
                do
                {
                    digits[count++] = static_cast<char>('0' + value % 10U);
                    value /= 10U;
                } while (value != 0U);
                while (count != 0U)
                    Put(digits[--count]);
            }

            void UnsignedFixed(std::uint32_t value, unsigned width)
            {
                char digits[10];
                unsigned count = 0U;
                do
                {
                    digits[count++] = static_cast<char>('0' + value % 10U);
                    value /= 10U;
                } while (value != 0U && count < sizeof(digits));
                const unsigned padding = width > count ? width - count : 0U;
                for (unsigned index = 0U; index < padding; ++index)
                    Put('0');
                while (count != 0U)
                    Put(digits[--count]);
            }

            void Signed(std::int32_t value)
            {
                if (value < 0)
                {
                    Put('-');
                    const std::uint32_t magnitude =
                        static_cast<std::uint32_t>(-static_cast<std::int64_t>(value));
                    Unsigned(magnitude);
                }
                else
                {
                    Unsigned(static_cast<std::uint32_t>(value));
                }
            }

            std::size_t Length() const { return _length; }

            void Terminate()
            {
                if (_buffer != nullptr && _capacity != 0U)
                {
                    const std::size_t end = _length < _capacity ? _length : _capacity - 1U;
                    _buffer[end] = '\0';
                }
            }

        private:
            void Put(char value)
            {
                if (_buffer != nullptr && _capacity > 1U && _length < _capacity - 1U)
                    _buffer[_length] = value;
                ++_length;
            }

            char* _buffer;
            std::size_t _capacity;
            std::size_t _length;
        };

        void AddField(Appender& output, const char* name, std::uint32_t value)
        {
            output.Text(" ");
            output.Text(name);
            output.Text("=");
            output.Unsigned(value);
        }
    } // namespace

    const char* StageName(std::uint32_t stage)
    {
        for (std::size_t index = 0U; index < kStageCount; ++index)
        {
            if (kStages[index].id == stage)
                return kStages[index].name;
        }
        return "UNKNOWN";
    }

    const char* StageName(Stage stage) { return StageName(StageValue(stage)); }

    bool IsKnownStage(std::uint32_t stage)
    {
        for (std::size_t index = 0U; index < kStageCount; ++index)
        {
            if (kStages[index].id == stage)
                return true;
        }
        return false;
    }

    Record EmptyRecord()
    {
        Record record = {};
        record.magic = kMagic;
        record.format_version = kFormatVersion;
        record.valid_marker = kInvalidMarker;
        record.stage = StageValue(Stage::NONE);
        record.stage_inverse = ~record.stage;
        record.socket = -1;
        record.configured_httpd_stack_bytes = 4096U;
        record.httpd_stack_high_water_raw = kUnmeasuredStack;
        record.httpd_stack_high_water_bytes = kUnmeasuredStack;
        record.httpd_stack_sample_valid = 0U;
        return record;
    }

    Record MakeRecord(Stage stage,
                      std::uint32_t sequence,
                      std::uint32_t serverGeneration,
                      std::uint32_t websocketGeneration,
                      std::int32_t socket,
                      std::uint32_t streamId,
                      std::uint32_t configuredStackBytes,
                      std::uint32_t stackRaw,
                      std::uint32_t stackBytes,
                      std::uint32_t stackSampleValid,
                      std::uint32_t heapFree,
                      std::uint32_t heapMin,
                      std::uint32_t heapLargest,
                      std::uint32_t resetReasonCode,
                      std::uint32_t resetReasonRaw)
    {
        Record record = EmptyRecord();
        record.sequence = sequence;
        record.stage = StageValue(stage);
        record.stage_inverse = ~record.stage;
        record.server_generation = serverGeneration;
        record.websocket_generation = websocketGeneration;
        record.socket = socket;
        record.stream_id = streamId;
        record.configured_httpd_stack_bytes = configuredStackBytes;
        record.httpd_stack_high_water_raw = stackRaw;
        record.httpd_stack_high_water_bytes = stackBytes;
        record.httpd_stack_sample_valid = stackSampleValid;
        record.internal_heap_free = heapFree;
        record.internal_heap_min = heapMin;
        record.internal_heap_largest = heapLargest;
        record.reset_reason_code = resetReasonCode;
        record.reset_reason_raw = resetReasonRaw;
        record.checksum = CanonicalChecksum(record);
        return record;
    }

    std::uint32_t CanonicalChecksum(const Record& record)
    {
        std::uint32_t crc = 0xFFFFFFFFU;
        // Explicit canonical little-endian field order.  valid_marker and
        // checksum are deliberately excluded from this sequence.
        crc = CrcWord(crc, record.magic);
        crc = CrcWord(crc, record.format_version);
        crc = CrcWord(crc, record.sequence);
        crc = CrcWord(crc, record.stage);
        crc = CrcWord(crc, record.stage_inverse);
        crc = CrcWord(crc, record.server_generation);
        crc = CrcWord(crc, record.websocket_generation);
        crc = CrcWord(crc, static_cast<std::uint32_t>(record.socket));
        crc = CrcWord(crc, record.stream_id);
        crc = CrcWord(crc, record.configured_httpd_stack_bytes);
        crc = CrcWord(crc, record.httpd_stack_high_water_raw);
        crc = CrcWord(crc, record.httpd_stack_high_water_bytes);
        crc = CrcWord(crc, record.internal_heap_free);
        crc = CrcWord(crc, record.internal_heap_min);
        crc = CrcWord(crc, record.internal_heap_largest);
        crc = CrcWord(crc, record.reset_reason_code);
        crc = CrcWord(crc, record.reset_reason_raw);
        crc = CrcWord(crc, record.httpd_stack_sample_valid);
        return FinishCrc(crc);
    }

    bool IsValid(const Record& record)
    {
        if (record.magic != kMagic || record.format_version != kFormatVersion ||
            record.valid_marker != kValidMarker || record.sequence == 0U ||
            record.stage_inverse != ~record.stage || record.checksum != CanonicalChecksum(record))
            return false;
        if (record.httpd_stack_sample_valid > 1U)
            return false;
        if (record.httpd_stack_sample_valid == 0U &&
            (record.httpd_stack_high_water_raw != kUnmeasuredStack ||
             record.httpd_stack_high_water_bytes != kUnmeasuredStack))
            return false;
        return true;
    }

    void Invalidate(Record& target) { target.valid_marker = kInvalidMarker; }

    void Commit(Record& target, const Record& source)
    {
        // A release fence models the RTC adapter's barrier before the body and
        // final marker stores.  The adapter repeats this sequence for volatile
        // RTC storage.
        target.valid_marker = kInvalidMarker;
        std::atomic_thread_fence(std::memory_order_release);
        target.magic = source.magic;
        target.format_version = source.format_version;
        target.sequence = source.sequence;
        target.stage = source.stage;
        target.stage_inverse = source.stage_inverse;
        target.server_generation = source.server_generation;
        target.websocket_generation = source.websocket_generation;
        target.socket = source.socket;
        target.stream_id = source.stream_id;
        target.configured_httpd_stack_bytes = source.configured_httpd_stack_bytes;
        target.httpd_stack_high_water_raw = source.httpd_stack_high_water_raw;
        target.httpd_stack_high_water_bytes = source.httpd_stack_high_water_bytes;
        target.internal_heap_free = source.internal_heap_free;
        target.internal_heap_min = source.internal_heap_min;
        target.internal_heap_largest = source.internal_heap_largest;
        target.reset_reason_code = source.reset_reason_code;
        target.reset_reason_raw = source.reset_reason_raw;
        target.httpd_stack_sample_valid = source.httpd_stack_sample_valid;
        target.checksum = CanonicalChecksum(target);
        std::atomic_thread_fence(std::memory_order_release);
        target.valid_marker = kValidMarker;
    }

    std::uint32_t NextSequence(std::uint32_t current, bool haveCurrent)
    {
        if (!haveCurrent || current == 0U)
            return 1U;
        return current == std::numeric_limits<std::uint32_t>::max() ? 1U : current + 1U;
    }

    bool IsSequenceNewer(std::uint32_t candidate, std::uint32_t incumbent)
    {
        if (candidate == 0U || incumbent == 0U || candidate == incumbent)
            return false;
        const std::uint32_t difference = candidate - incumbent;
        // Equal and exact half-range are deliberately ties; SelectNewestSlot
        // resolves those to slot 0.
        return difference != 0x80000000U && difference < 0x80000000U;
    }

    unsigned SelectNewestSlot(const Record& slot0, const Record& slot1, bool& haveValid)
    {
        const bool valid0 = IsValid(slot0);
        const bool valid1 = IsValid(slot1);
        haveValid = valid0 || valid1;
        if (!valid0 && !valid1)
            return 0U;
        if (!valid0)
            return 1U;
        if (!valid1)
            return 0U;
        return IsSequenceNewer(slot1.sequence, slot0.sequence) ? 1U : 0U;
    }

    bool CapturePriorSnapshot(const Record& slot0, const Record& slot1, Record& prior)
    {
        bool haveValid = false;
        const unsigned selected = SelectNewestSlot(slot0, slot1, haveValid);
        if (!haveValid)
            return false;
        prior = selected == 0U ? slot0 : slot1;
        return true;
    }

    std::size_t FormatCapturedPriorSnapshot(const Record& prior,
                                            bool havePrior,
                                            char* buffer,
                                            std::size_t capacity)
    {
        if (havePrior && IsValid(prior))
            return FormatRecord(prior, buffer, capacity);

        Appender output(buffer, capacity);
        output.Text("D2B_BREADCRUMB prior_snapshot=none");
        output.Terminate();
        return output.Length();
    }

    std::size_t FormatPriorSnapshot(const Record& slot0,
                                    const Record& slot1,
                                    char* buffer,
                                    std::size_t capacity)
    {
        Record prior = {};
        const bool havePrior = CapturePriorSnapshot(slot0, slot1, prior);
        return FormatCapturedPriorSnapshot(prior, havePrior, buffer, capacity);
    }

    std::size_t FormatRecord(const Record& record, char* buffer, std::size_t capacity)
    {
        Appender output(buffer, capacity);
        output.Text("D2B_BREADCRUMB stage=");
        if (record.stage <= 999U)
            output.UnsignedFixed(record.stage, 3U);
        else
            output.Unsigned(record.stage);
        output.Text(" stage_name=");
        output.Text(StageName(record.stage));
        AddField(output, "sequence", record.sequence);
        AddField(output, "server_generation", record.server_generation);
        AddField(output, "websocket_generation", record.websocket_generation);
        output.Text(" socket=");
        output.Signed(record.socket);
        AddField(output, "stream_id", record.stream_id);
        AddField(output, "configured_httpd_stack_bytes", record.configured_httpd_stack_bytes);
        AddField(output, "httpd_stack_sample_valid", record.httpd_stack_sample_valid);
        AddField(output, "httpd_stack_high_water_raw", record.httpd_stack_high_water_raw);
        AddField(output, "httpd_stack_high_water_bytes", record.httpd_stack_high_water_bytes);
        AddField(output, "internal_heap_free", record.internal_heap_free);
        AddField(output, "internal_heap_min", record.internal_heap_min);
        AddField(output, "internal_heap_largest", record.internal_heap_largest);
        AddField(output, "reset_reason_code", record.reset_reason_code);
        AddField(output, "reset_reason_raw", record.reset_reason_raw);
        AddField(output, "checksum", record.checksum);
        output.Terminate();
        return output.Length();
    }
} // namespace D2B_RESET_BREADCRUMB
