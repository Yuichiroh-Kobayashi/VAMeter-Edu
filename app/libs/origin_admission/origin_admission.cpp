#include "origin_admission.h"

namespace ORIGIN_ADMISSION
{
    namespace
    {
        enum Phase : std::uint8_t
        {
            Method,
            MethodSpace,
            TargetStart,
            Target,
            Version,
            RequestLineCr,
            RequestLineLf,
            HeaderLineStart,
            FieldName,
            ValueSpace,
            Value,
            FieldLf,
            HeadersEndLf,
            Opaque,
            Failed,
        };

        enum FieldKind : std::uint8_t
        {
            Generic,
            Origin,
            Upgrade,
            Connection,
            ContentLength,
            TransferEncoding,
        };

        enum Flag : std::uint8_t
        {
            StreamTarget = 1U << 0,
            SawOrigin = 1U << 1,
            SawUpgrade = 1U << 2,
            SawConnection = 1U << 3,
            TargetMatches = 1U << 4,
        };

        static const char kMethod[] = "GET";
        static const char kVersion[] = "HTTP/1.1";
        static const char kStreamTarget[] = "/d2b/v0/stream";
        static const char kUpgradeValue[] = "websocket";
        static const char kConnectionValue[] = "upgrade";
        static const char* const kFieldNames[] = {
            "origin",
            "upgrade",
            "connection",
            "content-length",
            "transfer-encoding",
        };
        static const std::uint8_t kFieldLengths[] = {6, 7, 10, 14, 17};

        std::uint8_t Lower(std::uint8_t byte)
        {
            return byte >= 'A' && byte <= 'Z' ? static_cast<std::uint8_t>(byte + ('a' - 'A')) : byte;
        }

        bool IsToken(std::uint8_t byte)
        {
            if ((byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z'))
            {
                return true;
            }
            switch (byte)
            {
            case '!':
            case '#':
            case '$':
            case '%':
            case '&':
            case '\'':
            case '*':
            case '+':
            case '-':
            case '.':
            case '^':
            case '_':
            case '`':
            case '|':
            case '~':
                return true;
            default:
                return false;
            }
        }

        void ResetRequest(State& state)
        {
            const Policy* policy = state.policy;
            state = State();
            state.policy = policy;
            state.phase = Method;
        }

        Result MakeResult(Decision decision,
                          RejectReason reason,
                          std::size_t consumed,
                          std::size_t length,
                          std::uint32_t normal_requests,
                          std::uint32_t header_bytes = 0)
        {
            Result result = {decision, reason, consumed, consumed, length - consumed, normal_requests, header_bytes};
            return result;
        }

        Result
        Reject(State& state, RejectReason reason, std::size_t consumed, std::size_t length, std::uint32_t normal_requests)
        {
            state.phase = Failed;
            state.reject_reason = reason;
            return MakeResult(Decision::Rejected, reason, consumed, length, normal_requests);
        }

        FieldKind ClassifyField(const State& state)
        {
            for (std::uint8_t index = 0; index < 5; ++index)
            {
                if ((state.field_candidates & (1U << index)) != 0 && state.field_name_length == kFieldLengths[index])
                {
                    return static_cast<FieldKind>(index + 1);
                }
            }
            return Generic;
        }

        void AddFieldNameByte(State& state, std::uint8_t byte)
        {
            for (std::uint8_t index = 0; index < 5; ++index)
            {
                if ((state.field_candidates & (1U << index)) != 0 &&
                    (state.field_name_length >= kFieldLengths[index] ||
                     Lower(byte) != static_cast<std::uint8_t>(kFieldNames[index][state.field_name_length])))
                {
                    state.field_candidates &= static_cast<std::uint8_t>(~(1U << index));
                }
            }
            ++state.field_name_length;
        }

        RejectReason BeginValue(State& state)
        {
            state.field_kind = ClassifyField(state);
            if (state.field_kind == ContentLength)
                return RejectReason::ContentLength;
            if (state.field_kind == TransferEncoding)
                return RejectReason::TransferEncoding;

            const bool stream = (state.flags & StreamTarget) != 0;
            if (!stream && state.field_kind == Upgrade)
                return RejectReason::WrongWebSocketTarget;

            std::uint8_t flag = 0;
            RejectReason duplicate = RejectReason::None;
            if (stream && state.field_kind == Origin)
            {
                flag = SawOrigin;
                duplicate = RejectReason::DuplicateOrigin;
            }
            else if (stream && state.field_kind == Upgrade)
            {
                flag = SawUpgrade;
                duplicate = RejectReason::DuplicateUpgrade;
            }
            else if (stream && state.field_kind == Connection)
            {
                flag = SawConnection;
                duplicate = RejectReason::DuplicateConnection;
            }
            else
            {
                state.field_kind = Generic;
            }
            if (flag != 0 && (state.flags & flag) != 0)
                return duplicate;
            state.flags |= flag;
            state.value_length = 0;
            state.value_index = 0;
            state.value_matches = 1;
            state.null_candidate = 1;
            return RejectReason::None;
        }

        RejectReason AddValueByte(State& state, std::uint8_t byte)
        {
            if (byte < 0x20 || byte > 0x7e)
                return RejectReason::MalformedHeader;
            if (++state.value_length > kMaximumFieldValueBytes)
                return RejectReason::FieldValueTooLong;

            const std::uint16_t index = state.value_index++;
            if (state.field_kind == Origin)
            {
                if (state.value_length > kMaximumOriginBytes)
                    return RejectReason::OriginTooLong;
                if (byte == ',')
                    return RejectReason::OriginComma;
                if (byte == ' ' || byte == '\t')
                    return RejectReason::MalformedOrigin;
                if (index >= state.policy->expected_origin_length ||
                    byte != static_cast<std::uint8_t>(state.policy->expected_origin[index]))
                {
                    state.value_matches = 0;
                }
                static const char kNull[] = "null";
                if (index >= 4 || byte != static_cast<std::uint8_t>(kNull[index]))
                    state.null_candidate = 0;
            }
            else if (state.field_kind == Upgrade || state.field_kind == Connection)
            {
                const char* expected = state.field_kind == Upgrade ? kUpgradeValue : kConnectionValue;
                const std::size_t expected_length =
                    state.field_kind == Upgrade ? sizeof(kUpgradeValue) - 1 : sizeof(kConnectionValue) - 1;
                if (byte == ',' || byte == ' ' || index >= expected_length || Lower(byte) != expected[index])
                    state.value_matches = 0;
            }
            return RejectReason::None;
        }

        RejectReason FinishValue(const State& state)
        {
            if (state.field_kind == Origin)
            {
                if (state.value_length == 0)
                    return RejectReason::EmptyOrigin;
                if (state.null_candidate != 0 && state.value_length == 4)
                    return RejectReason::NullOrigin;
                if (state.value_matches == 0 || state.value_length != state.policy->expected_origin_length)
                    return RejectReason::OriginMismatch;
            }
            else if (state.field_kind == Upgrade &&
                     (state.value_matches == 0 || state.value_length != sizeof(kUpgradeValue) - 1))
            {
                return RejectReason::InvalidUpgrade;
            }
            else if (state.field_kind == Connection &&
                     (state.value_matches == 0 || state.value_length != sizeof(kConnectionValue) - 1))
            {
                return RejectReason::InvalidConnection;
            }
            return RejectReason::None;
        }

        RejectReason FinishHeaders(const State& state)
        {
            if ((state.flags & StreamTarget) == 0)
                return RejectReason::None;
            if ((state.flags & SawOrigin) == 0)
                return RejectReason::MissingOrigin;
            if ((state.flags & SawUpgrade) == 0)
                return RejectReason::MissingUpgrade;
            if ((state.flags & SawConnection) == 0)
                return RejectReason::MissingConnection;
            return RejectReason::None;
        }

        bool ValidPolicy(const Policy& policy)
        {
            if (policy.expected_origin == 0 || policy.expected_origin_length < 8 ||
                policy.expected_origin_length > kMaximumOriginBytes)
                return false;
            static const char kPrefix[] = "http://";
            for (std::size_t index = 0; index < sizeof(kPrefix) - 1; ++index)
                if (policy.expected_origin[index] != kPrefix[index])
                    return false;
            for (std::size_t index = sizeof(kPrefix) - 1; index < policy.expected_origin_length; ++index)
            {
                const unsigned char byte = static_cast<unsigned char>(policy.expected_origin[index]);
                if (byte <= 0x20 || byte > 0x7e || byte == ',' || byte == '/' || byte == '?' || byte == '#' || byte == '@')
                    return false;
            }
            return true;
        }
    } // namespace

    bool Initialize(State& state, const Policy& policy)
    {
        state = State();
        state.policy = &policy;
        state.phase = Method;
        if (!ValidPolicy(policy))
        {
            state.phase = Failed;
            state.reject_reason = RejectReason::InvalidPolicy;
            return false;
        }
        return true;
    }

    bool Reset(State& state, const Policy& policy) { return Initialize(state, policy); }

    Result Consume(State& state, const std::uint8_t* bytes, std::size_t length)
    {
        if (state.phase == Failed)
            return MakeResult(Decision::Rejected, state.reject_reason, 0, length, 0);
        if (state.phase == Opaque)
            return MakeResult(Decision::AcceptedWebSocket, RejectReason::None, 0, length, 0);
        if (bytes == 0 && length != 0)
            return Reject(state, RejectReason::MalformedHeader, 0, length, 0);

        std::uint32_t normal_requests = 0;
        bool ended_normal = false;
        for (std::size_t offset = 0; offset < length; ++offset)
        {
            ended_normal = false;
            const std::uint8_t byte = bytes[offset];
            if (++state.request_bytes > kMaximumHeaderBytes)
                return Reject(state, RejectReason::HeaderTooLong, offset + 1, length, normal_requests);
            RejectReason error = RejectReason::None;
            switch (state.phase)
            {
            case Method:
                if (byte != static_cast<std::uint8_t>(kMethod[state.literal_index]))
                    error = RejectReason::Method;
                else if (++state.literal_index == sizeof(kMethod) - 1)
                    state.phase = MethodSpace;
                break;
            case MethodSpace:
                if (byte != ' ')
                    error = RejectReason::RequestLine;
                else
                    state.phase = TargetStart;
                break;
            case TargetStart:
                if (byte != '/')
                    error = RejectReason::RequestTarget;
                else
                {
                    state.phase = Target;
                    state.target_length = 1;
                    state.flags |= TargetMatches;
                }
                break;
            case Target:
                if (byte == ' ')
                {
                    if ((state.flags & TargetMatches) != 0 && state.target_length == sizeof(kStreamTarget) - 1)
                        state.flags |= StreamTarget;
                    state.phase = Version;
                    state.literal_index = 0;
                }
                else if (byte < 0x21 || byte > 0x7e || byte == '#')
                    error = RejectReason::RequestTarget;
                else if (++state.target_length > kMaximumTargetBytes)
                    error = RejectReason::RequestTarget;
                else if (state.target_length > sizeof(kStreamTarget) - 1 ||
                         byte != static_cast<std::uint8_t>(kStreamTarget[state.target_length - 1]))
                    state.flags &= static_cast<std::uint8_t>(~TargetMatches);
                break;
            case Version:
                if (byte != static_cast<std::uint8_t>(kVersion[state.literal_index]))
                    error = RejectReason::Version;
                else if (++state.literal_index == sizeof(kVersion) - 1)
                    state.phase = RequestLineCr;
                break;
            case RequestLineCr:
                if (byte != '\r')
                    error = byte == '\n' ? RejectReason::BareLf : RejectReason::RequestLine;
                else
                    state.phase = RequestLineLf;
                break;
            case RequestLineLf:
                if (byte != '\n')
                    error = RejectReason::RequestLine;
                else
                    state.phase = HeaderLineStart;
                break;
            case HeaderLineStart:
                if (byte == '\r')
                    state.phase = HeadersEndLf;
                else if (byte == '\n')
                    error = RejectReason::BareLf;
                else if (byte == ' ' || byte == '\t')
                    error = RejectReason::ObsFold;
                else if (!IsToken(byte))
                    error = RejectReason::MalformedHeader;
                else
                {
                    state.field_candidates = 0x1f;
                    state.field_name_length = 0;
                    AddFieldNameByte(state, byte);
                    state.phase = FieldName;
                }
                break;
            case FieldName:
                if (byte == ':')
                {
                    error = BeginValue(state);
                    state.phase = ValueSpace;
                }
                else if (!IsToken(byte))
                    error = byte == '\n' ? RejectReason::BareLf : RejectReason::MalformedHeader;
                else if (state.field_name_length >= kMaximumFieldNameBytes)
                    error = RejectReason::FieldNameTooLong;
                else
                    AddFieldNameByte(state, byte);
                break;
            case ValueSpace:
                if (byte != ' ')
                    error = byte == '\n' ? RejectReason::BareLf : RejectReason::MalformedHeader;
                else
                    state.phase = Value;
                break;
            case Value:
                if (byte == '\r')
                {
                    error = FinishValue(state);
                    state.phase = FieldLf;
                }
                else if (byte == '\n')
                    error = RejectReason::BareLf;
                else
                    error = AddValueByte(state, byte);
                break;
            case FieldLf:
                if (byte != '\n')
                    error = RejectReason::MalformedHeader;
                else
                    state.phase = HeaderLineStart;
                break;
            case HeadersEndLf:
                if (byte != '\n')
                    error = RejectReason::MalformedHeader;
                else
                {
                    error = FinishHeaders(state);
                    if (error == RejectReason::None && (state.flags & StreamTarget) != 0)
                    {
                        const std::uint32_t header_bytes = state.request_bytes;
                        state.phase = Opaque;
                        return MakeResult(
                            Decision::AcceptedWebSocket, RejectReason::None, offset + 1, length, normal_requests, header_bytes);
                    }
                    if (error == RejectReason::None)
                    {
                        ++normal_requests;
                        ended_normal = true;
                        ResetRequest(state);
                    }
                }
                break;
            case Opaque:
            case Failed:
                break;
            }
            if (error != RejectReason::None)
                return Reject(state, error, offset + 1, length, normal_requests);
        }
        return MakeResult(ended_normal ? Decision::AcceptedNormalHttp : Decision::NeedMore,
                          RejectReason::None,
                          length,
                          length,
                          normal_requests);
    }
} // namespace ORIGIN_ADMISSION
