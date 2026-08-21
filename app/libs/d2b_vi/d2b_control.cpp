#include "d2b_control.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace D2B
{
    namespace
    {
        static const std::size_t kMaximumDepth = 8;
        static const std::size_t kMaximumNodes = 128;
        static const std::size_t kMaximumMembers = 96;
        static const std::size_t kMaximumArrayItems = 96;
        static const std::size_t kMaximumMembersPerObject = 32;
        static const std::size_t kMaximumDecodedStringBytes = 1024;
        static const std::size_t kMaximumDecodedStringCodePoints = 512;
        static const std::size_t kStringPoolSize = 2176;
        static const std::uint16_t kNoIndex = 0xffff;

        enum class NodeType
        {
            Object,
            Array,
            String,
            Number,
            Boolean,
            Null,
        };

        struct StringRef
        {
            std::uint16_t offset;
            std::uint16_t bytes;
            std::uint16_t codePoints;
        };

        struct Node
        {
            NodeType type;
            std::uint16_t first;
            std::uint16_t count;
            StringRef text;
            bool integerToken;
            bool nonNegativeInteger;
            std::uint64_t unsignedValue;
        };

        struct Member
        {
            StringRef key;
            std::uint16_t value;
            std::uint16_t next;
        };

        struct ArrayItem
        {
            std::uint16_t value;
            std::uint16_t next;
        };

        struct Document
        {
            Node nodes[kMaximumNodes];
            Member members[kMaximumMembers];
            ArrayItem items[kMaximumArrayItems];
            char strings[kStringPoolSize];
            std::uint16_t nodeCount;
            std::uint16_t memberCount;
            std::uint16_t itemCount;
            std::uint16_t stringBytes;
            std::uint16_t root;
        };

        bool IsWhitespace(std::uint8_t value)
        {
            return value == ' ' || value == '\t' || value == '\r' || value == '\n';
        }

        bool IsDigit(std::uint8_t value) { return value >= '0' && value <= '9'; }

        bool IsHex(std::uint8_t value)
        {
            return IsDigit(value) || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F');
        }

        std::uint32_t HexValue(std::uint8_t value)
        {
            if (IsDigit(value))
                return value - '0';
            if (value >= 'a' && value <= 'f')
                return value - 'a' + 10;
            return value - 'A' + 10;
        }

        std::size_t Utf8SequenceLength(std::uint8_t lead)
        {
            if (lead < 0x80)
                return 1;
            if (lead >= 0xc2 && lead <= 0xdf)
                return 2;
            if (lead >= 0xe0 && lead <= 0xef)
                return 3;
            if (lead >= 0xf0 && lead <= 0xf4)
                return 4;
            return 0;
        }

        bool IsStrictUtf8(const std::uint8_t* data, std::size_t size)
        {
            for (std::size_t index = 0; index < size;)
            {
                const std::uint8_t lead = data[index];
                const std::size_t length = Utf8SequenceLength(lead);
                if (length == 0 || index + length > size)
                    return false;
                if (length == 1)
                {
                    ++index;
                    continue;
                }
                for (std::size_t offset = 1; offset < length; ++offset)
                {
                    if ((data[index + offset] & 0xc0) != 0x80)
                        return false;
                }
                if (length == 3)
                {
                    const std::uint8_t second = data[index + 1];
                    if ((lead == 0xe0 && second < 0xa0) || (lead == 0xed && second >= 0xa0))
                        return false;
                }
                if (length == 4)
                {
                    const std::uint8_t second = data[index + 1];
                    if ((lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second >= 0x90))
                        return false;
                }
                index += length;
            }
            return true;
        }

        class Parser
        {
        public:
            ErrorCode parse(const std::uint8_t* data, std::size_t size, Document& document)
            {
                _data = data;
                _size = size;
                _index = 0;
                _document = &document;
                std::memset(_document, 0, sizeof(*_document));
                if (size > kMaximumControlMessageSize)
                    return ErrorCode::FrameTooLarge;
                if (data == nullptr || !IsStrictUtf8(data, size))
                    return ErrorCode::InvalidMessage;

                skipWhitespace();
                ErrorCode error = parseValue(0, _document->root);
                if (error != ErrorCode::None)
                    return error;
                skipWhitespace();
                return _index == _size ? ErrorCode::None : ErrorCode::InvalidMessage;
            }

        private:
            const std::uint8_t* _data;
            std::size_t _size;
            std::size_t _index;
            Document* _document;

            void skipWhitespace()
            {
                while (_index < _size && IsWhitespace(_data[_index]))
                    ++_index;
            }

            ErrorCode addNode(NodeType type, std::uint16_t& output)
            {
                if (_document->nodeCount >= kMaximumNodes)
                    return ErrorCode::InvalidMessage;
                output = _document->nodeCount++;
                Node& node = _document->nodes[output];
                std::memset(&node, 0, sizeof(node));
                node.type = type;
                node.first = kNoIndex;
                return ErrorCode::None;
            }

            ErrorCode parseValue(std::size_t depth, std::uint16_t& output)
            {
                if (depth > kMaximumDepth)
                    return ErrorCode::InvalidMessage;
                skipWhitespace();
                if (_index >= _size)
                    return ErrorCode::InvalidMessage;
                const std::uint8_t value = _data[_index];
                if (value == '{')
                    return parseObject(depth, output);
                if (value == '[')
                    return parseArray(depth, output);
                if (value == '"')
                    return parseStringNode(output);
                if (value == 't')
                    return parseLiteral("true", NodeType::Boolean, output);
                if (value == 'f')
                    return parseLiteral("false", NodeType::Boolean, output);
                if (value == 'n')
                    return parseLiteral("null", NodeType::Null, output);
                if (value == '-' || IsDigit(value))
                    return parseNumber(output);
                return ErrorCode::InvalidMessage;
            }

            ErrorCode parseObject(std::size_t depth, std::uint16_t& output)
            {
                ErrorCode error = addNode(NodeType::Object, output);
                if (error != ErrorCode::None)
                    return error;
                ++_index;
                skipWhitespace();
                if (_index < _size && _data[_index] == '}')
                {
                    ++_index;
                    return ErrorCode::None;
                }

                std::uint16_t last = kNoIndex;
                while (true)
                {
                    Node& object = _document->nodes[output];
                    if (object.count >= kMaximumMembersPerObject || _document->memberCount >= kMaximumMembers)
                        return ErrorCode::InvalidMessage;
                    StringRef key = {};
                    error = parseString(key);
                    if (error != ErrorCode::None)
                        return error;
                    for (std::uint16_t member = object.first; member != kNoIndex; member = _document->members[member].next)
                    {
                        if (equal(_document->members[member].key, key))
                            return ErrorCode::InvalidMessage;
                    }
                    skipWhitespace();
                    if (_index >= _size || _data[_index] != ':')
                        return ErrorCode::InvalidMessage;
                    ++_index;
                    std::uint16_t child = 0;
                    error = parseValue(depth + 1, child);
                    if (error != ErrorCode::None)
                        return error;

                    const std::uint16_t memberIndex = _document->memberCount++;
                    Member& stored = _document->members[memberIndex];
                    stored.key = key;
                    stored.value = child;
                    stored.next = kNoIndex;
                    if (last == kNoIndex)
                        object.first = memberIndex;
                    else
                        _document->members[last].next = memberIndex;
                    last = memberIndex;
                    ++object.count;

                    skipWhitespace();
                    if (_index < _size && _data[_index] == '}')
                    {
                        ++_index;
                        return ErrorCode::None;
                    }
                    if (_index >= _size || _data[_index] != ',')
                        return ErrorCode::InvalidMessage;
                    ++_index;
                    skipWhitespace();
                }
            }

            ErrorCode parseArray(std::size_t depth, std::uint16_t& output)
            {
                ErrorCode error = addNode(NodeType::Array, output);
                if (error != ErrorCode::None)
                    return error;
                ++_index;
                skipWhitespace();
                if (_index < _size && _data[_index] == ']')
                {
                    ++_index;
                    return ErrorCode::None;
                }

                std::uint16_t last = kNoIndex;
                while (true)
                {
                    if (_document->itemCount >= kMaximumArrayItems)
                        return ErrorCode::InvalidMessage;
                    std::uint16_t child = 0;
                    error = parseValue(depth + 1, child);
                    if (error != ErrorCode::None)
                        return error;
                    Node& array = _document->nodes[output];
                    const std::uint16_t itemIndex = _document->itemCount++;
                    ArrayItem& stored = _document->items[itemIndex];
                    stored.value = child;
                    stored.next = kNoIndex;
                    if (last == kNoIndex)
                        array.first = itemIndex;
                    else
                        _document->items[last].next = itemIndex;
                    last = itemIndex;
                    ++array.count;

                    skipWhitespace();
                    if (_index < _size && _data[_index] == ']')
                    {
                        ++_index;
                        return ErrorCode::None;
                    }
                    if (_index >= _size || _data[_index] != ',')
                        return ErrorCode::InvalidMessage;
                    ++_index;
                }
            }

            ErrorCode appendCodePoint(std::uint32_t codePoint, std::size_t& bytes, std::size_t& codePoints)
            {
                std::uint8_t encoded[4];
                std::size_t length = 0;
                if (codePoint <= 0x7f)
                {
                    encoded[0] = static_cast<std::uint8_t>(codePoint);
                    length = 1;
                }
                else if (codePoint <= 0x7ff)
                {
                    encoded[0] = 0xc0 | static_cast<std::uint8_t>(codePoint >> 6);
                    encoded[1] = 0x80 | static_cast<std::uint8_t>(codePoint & 0x3f);
                    length = 2;
                }
                else if (codePoint <= 0xffff)
                {
                    encoded[0] = 0xe0 | static_cast<std::uint8_t>(codePoint >> 12);
                    encoded[1] = 0x80 | static_cast<std::uint8_t>((codePoint >> 6) & 0x3f);
                    encoded[2] = 0x80 | static_cast<std::uint8_t>(codePoint & 0x3f);
                    length = 3;
                }
                else
                {
                    encoded[0] = 0xf0 | static_cast<std::uint8_t>(codePoint >> 18);
                    encoded[1] = 0x80 | static_cast<std::uint8_t>((codePoint >> 12) & 0x3f);
                    encoded[2] = 0x80 | static_cast<std::uint8_t>((codePoint >> 6) & 0x3f);
                    encoded[3] = 0x80 | static_cast<std::uint8_t>(codePoint & 0x3f);
                    length = 4;
                }
                if (bytes + length > kMaximumDecodedStringBytes || codePoints + 1 > kMaximumDecodedStringCodePoints ||
                    _document->stringBytes + bytes + length + 1 > kStringPoolSize)
                    return ErrorCode::InvalidMessage;
                std::memcpy(_document->strings + _document->stringBytes + bytes, encoded, length);
                bytes += length;
                ++codePoints;
                return ErrorCode::None;
            }

            ErrorCode parseUnicodeEscape(std::uint32_t& codePoint)
            {
                if (_index + 4 > _size)
                    return ErrorCode::InvalidMessage;
                codePoint = 0;
                for (std::size_t count = 0; count < 4; ++count)
                {
                    if (!IsHex(_data[_index]))
                        return ErrorCode::InvalidMessage;
                    codePoint = (codePoint << 4) | HexValue(_data[_index++]);
                }
                if (codePoint >= 0xd800 && codePoint <= 0xdbff)
                {
                    if (_index + 6 > _size || _data[_index] != '\\' || _data[_index + 1] != 'u')
                        return ErrorCode::InvalidMessage;
                    _index += 2;
                    std::uint32_t low = 0;
                    for (std::size_t count = 0; count < 4; ++count)
                    {
                        if (!IsHex(_data[_index]))
                            return ErrorCode::InvalidMessage;
                        low = (low << 4) | HexValue(_data[_index++]);
                    }
                    if (low < 0xdc00 || low > 0xdfff)
                        return ErrorCode::InvalidMessage;
                    codePoint = 0x10000 + ((codePoint - 0xd800) << 10) + (low - 0xdc00);
                }
                else if (codePoint >= 0xdc00 && codePoint <= 0xdfff)
                {
                    return ErrorCode::InvalidMessage;
                }
                return ErrorCode::None;
            }

            ErrorCode parseString(StringRef& output)
            {
                skipWhitespace();
                if (_index >= _size || _data[_index] != '"')
                    return ErrorCode::InvalidMessage;
                ++_index;
                const std::size_t start = _document->stringBytes;
                std::size_t bytes = 0;
                std::size_t codePoints = 0;
                while (_index < _size)
                {
                    const std::uint8_t value = _data[_index++];
                    if (value == '"')
                    {
                        if (start + bytes >= kStringPoolSize)
                            return ErrorCode::InvalidMessage;
                        _document->strings[start + bytes] = '\0';
                        _document->stringBytes = static_cast<std::uint16_t>(start + bytes + 1);
                        output.offset = static_cast<std::uint16_t>(start);
                        output.bytes = static_cast<std::uint16_t>(bytes);
                        output.codePoints = static_cast<std::uint16_t>(codePoints);
                        return ErrorCode::None;
                    }
                    if (value < 0x20)
                        return ErrorCode::InvalidMessage;
                    if (value == '\\')
                    {
                        if (_index >= _size)
                            return ErrorCode::InvalidMessage;
                        const std::uint8_t escape = _data[_index++];
                        std::uint32_t codePoint = 0;
                        switch (escape)
                        {
                            case '"': codePoint = '"'; break;
                            case '\\': codePoint = '\\'; break;
                            case '/': codePoint = '/'; break;
                            case 'b': codePoint = '\b'; break;
                            case 'f': codePoint = '\f'; break;
                            case 'n': codePoint = '\n'; break;
                            case 'r': codePoint = '\r'; break;
                            case 't': codePoint = '\t'; break;
                            case 'u':
                            {
                                const ErrorCode error = parseUnicodeEscape(codePoint);
                                if (error != ErrorCode::None)
                                    return error;
                                break;
                            }
                            default: return ErrorCode::InvalidMessage;
                        }
                        const ErrorCode error = appendCodePoint(codePoint, bytes, codePoints);
                        if (error != ErrorCode::None)
                            return error;
                        continue;
                    }

                    const std::size_t length = Utf8SequenceLength(value);
                    if (length == 0 || _index - 1 + length > _size || bytes + length > kMaximumDecodedStringBytes ||
                        codePoints + 1 > kMaximumDecodedStringCodePoints || start + bytes + length + 1 > kStringPoolSize)
                        return ErrorCode::InvalidMessage;
                    _document->strings[start + bytes++] = static_cast<char>(value);
                    for (std::size_t offset = 1; offset < length; ++offset)
                        _document->strings[start + bytes++] = static_cast<char>(_data[_index++]);
                    ++codePoints;
                }
                return ErrorCode::InvalidMessage;
            }

            ErrorCode parseStringNode(std::uint16_t& output)
            {
                ErrorCode error = addNode(NodeType::String, output);
                if (error != ErrorCode::None)
                    return error;
                return parseString(_document->nodes[output].text);
            }

            ErrorCode parseLiteral(const char* literal, NodeType type, std::uint16_t& output)
            {
                const std::size_t length = std::strlen(literal);
                if (_index + length > _size || std::memcmp(_data + _index, literal, length) != 0)
                    return ErrorCode::InvalidMessage;
                _index += length;
                return addNode(type, output);
            }

            ErrorCode parseNumber(std::uint16_t& output)
            {
                const std::size_t start = _index;
                bool negative = false;
                if (_data[_index] == '-')
                {
                    negative = true;
                    ++_index;
                    if (_index >= _size)
                        return ErrorCode::InvalidMessage;
                }
                if (_data[_index] == '0')
                {
                    ++_index;
                    if (_index < _size && IsDigit(_data[_index]))
                        return ErrorCode::InvalidMessage;
                }
                else
                {
                    if (_data[_index] < '1' || _data[_index] > '9')
                        return ErrorCode::InvalidMessage;
                    while (_index < _size && IsDigit(_data[_index]))
                        ++_index;
                }
                bool integerToken = true;
                if (_index < _size && _data[_index] == '.')
                {
                    integerToken = false;
                    ++_index;
                    if (_index >= _size || !IsDigit(_data[_index]))
                        return ErrorCode::InvalidMessage;
                    while (_index < _size && IsDigit(_data[_index]))
                        ++_index;
                }
                if (_index < _size && (_data[_index] == 'e' || _data[_index] == 'E'))
                {
                    integerToken = false;
                    ++_index;
                    if (_index < _size && (_data[_index] == '+' || _data[_index] == '-'))
                        ++_index;
                    if (_index >= _size || !IsDigit(_data[_index]))
                        return ErrorCode::InvalidMessage;
                    while (_index < _size && IsDigit(_data[_index]))
                        ++_index;
                }

                const std::size_t length = _index - start;
                if (length == 0 || length > 64)
                    return ErrorCode::InvalidMessage;
                char token[65];
                std::memcpy(token, _data + start, length);
                token[length] = '\0';
                char* end = nullptr;
                const double value = std::strtod(token, &end);
                if (end != token + length || !std::isfinite(value))
                    return ErrorCode::InvalidMessage;

                ErrorCode error = addNode(NodeType::Number, output);
                if (error != ErrorCode::None)
                    return error;
                Node& node = _document->nodes[output];
                node.integerToken = integerToken;
                node.nonNegativeInteger = integerToken;
                node.unsignedValue = 0;
                std::size_t digit = negative ? start + 1 : start;
                while (integerToken && digit < _index)
                {
                    const std::uint64_t next = _data[digit++] - '0';
                    if (node.unsignedValue > (std::numeric_limits<std::uint64_t>::max() - next) / 10)
                    {
                        node.nonNegativeInteger = false;
                        break;
                    }
                    node.unsignedValue = node.unsignedValue * 10 + next;
                }
                if (negative && node.unsignedValue != 0)
                    node.nonNegativeInteger = false;
                return ErrorCode::None;
            }

            bool equal(StringRef left, StringRef right) const
            {
                return left.bytes == right.bytes &&
                       std::memcmp(_document->strings + left.offset, _document->strings + right.offset, left.bytes) == 0;
            }
        };

        const char* StringValue(const Document& document, const Node& node)
        {
            return document.strings + node.text.offset;
        }

        bool StringEquals(const Document& document, const Node& node, const char* expected)
        {
            if (node.type != NodeType::String)
                return false;
            const std::size_t length = std::strlen(expected);
            return node.text.bytes == length && std::memcmp(StringValue(document, node), expected, length) == 0;
        }

        const Node* Find(const Document& document, const Node& object, const char* key)
        {
            if (object.type != NodeType::Object)
                return nullptr;
            const std::size_t keyLength = std::strlen(key);
            for (std::uint16_t index = object.first; index != kNoIndex; index = document.members[index].next)
            {
                const Member& member = document.members[index];
                if (member.key.bytes == keyLength && std::memcmp(document.strings + member.key.offset, key, keyLength) == 0)
                    return &document.nodes[member.value];
            }
            return nullptr;
        }

        bool HasExactFields(const Document& document,
                            const Node& object,
                            const char* const* required,
                            std::size_t requiredCount,
                            const char* const* optional,
                            std::size_t optionalCount)
        {
            if (object.type != NodeType::Object)
                return false;
            if (object.count < requiredCount || object.count > requiredCount + optionalCount)
                return false;
            for (std::size_t index = 0; index < requiredCount; ++index)
            {
                if (Find(document, object, required[index]) == nullptr)
                    return false;
            }
            for (std::uint16_t index = object.first; index != kNoIndex; index = document.members[index].next)
            {
                const Member& member = document.members[index];
                bool allowed = false;
                for (std::size_t requiredIndex = 0; requiredIndex < requiredCount; ++requiredIndex)
                {
                    const std::size_t length = std::strlen(required[requiredIndex]);
                    if (member.key.bytes == length && std::memcmp(document.strings + member.key.offset, required[requiredIndex], length) == 0)
                        allowed = true;
                }
                for (std::size_t optionalIndex = 0; optionalIndex < optionalCount; ++optionalIndex)
                {
                    const std::size_t length = std::strlen(optional[optionalIndex]);
                    if (member.key.bytes == length && std::memcmp(document.strings + member.key.offset, optional[optionalIndex], length) == 0)
                        allowed = true;
                }
                if (!allowed)
                    return false;
            }
            return true;
        }

        bool IsString(const Node* node, std::size_t minimum, std::size_t maximum)
        {
            return node != nullptr && node->type == NodeType::String && node->text.codePoints >= minimum &&
                   node->text.codePoints <= maximum;
        }

        bool IsInteger(const Node* node, std::uint64_t minimum, std::uint64_t maximum)
        {
            return node != nullptr && node->type == NodeType::Number && node->integerToken && node->nonNegativeInteger &&
                   node->unsignedValue >= minimum && node->unsignedValue <= maximum;
        }

        bool IsIdentifier(const Document& document, const Node* node)
        {
            if (!IsString(node, 1, 128))
                return false;
            const char* value = StringValue(document, *node);
            for (std::size_t index = 0; index < node->text.bytes; ++index)
            {
                const char character = value[index];
                const bool alphaNumeric = (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9');
                if ((!alphaNumeric && character != '.' && character != '_' && character != '-') || (index == 0 && !alphaNumeric))
                    return false;
            }
            return true;
        }

        bool CopyString(const Document& document, const Node& node, char* output, std::size_t capacity)
        {
            if (node.type != NodeType::String || static_cast<std::size_t>(node.text.bytes) + 1 > capacity)
                return false;
            std::memcpy(output, StringValue(document, node), node.text.bytes + 1);
            return true;
        }

        bool IsVersion(const Document& document, const Node& node)
        {
            if (!IsString(&node, 3, kMaximumDecodedStringCodePoints))
                return false;
            const char* value = StringValue(document, node);
            std::size_t index = 0;
            for (int part = 0; part < 2; ++part)
            {
                if (index >= node.text.bytes || !IsDigit(static_cast<std::uint8_t>(value[index])))
                    return false;
                if (value[index] == '0' && index + 1 < node.text.bytes && value[index + 1] != '.')
                    return false;
                while (index < node.text.bytes && IsDigit(static_cast<std::uint8_t>(value[index])))
                    ++index;
                if (part == 0)
                {
                    if (index >= node.text.bytes || value[index++] != '.')
                        return false;
                }
            }
            return index == node.text.bytes;
        }

        ErrorCode ValidateParameterShape(const Document& document, const Node& parameters)
        {
            if (parameters.type != NodeType::Object || parameters.count > 32)
                return ErrorCode::InvalidMessage;
            for (std::uint16_t index = parameters.first; index != kNoIndex; index = document.members[index].next)
            {
                const Member& member = document.members[index];
                Node keyNode = {};
                keyNode.type = NodeType::String;
                keyNode.text = member.key;
                if (!IsIdentifier(document, &keyNode))
                    return ErrorCode::InvalidMessage;
            }
            const Node* sampleFormat = Find(document, parameters, "sample_format");
            const Node* channelCount = Find(document, parameters, "channel_count");
            const Node* channelMask = Find(document, parameters, "channel_mask");
            const Node* sampleRate = Find(document, parameters, "sample_rate");
            const Node* samplesPerFrame = Find(document, parameters, "samples_per_frame");
            if (sampleFormat != nullptr && !IsString(sampleFormat, 1, 64))
                return ErrorCode::InvalidMessage;
            if (channelCount != nullptr && !IsInteger(channelCount, 1, 32))
                return ErrorCode::InvalidMessage;
            if (channelMask != nullptr && !IsInteger(channelMask, 0, 0xffffffffULL))
                return ErrorCode::InvalidMessage;
            if (samplesPerFrame != nullptr && !IsInteger(samplesPerFrame, 1, 0xffffffffULL))
                return ErrorCode::InvalidMessage;
            if (sampleRate != nullptr)
            {
                static const char* const required[] = {"numerator", "denominator"};
                if (!HasExactFields(document, *sampleRate, required, 2, nullptr, 0) ||
                    !IsInteger(Find(document, *sampleRate, "numerator"), 0, 0xffffffffULL) ||
                    !IsInteger(Find(document, *sampleRate, "denominator"), 0, 0xffffffffULL))
                    return ErrorCode::InvalidMessage;
            }
            return ErrorCode::None;
        }

        ErrorCode ValidateParameters(const Document& document, const Node& parameters, Profile profile)
        {
            const ErrorCode shape = ValidateParameterShape(document, parameters);
            if (shape != ErrorCode::None)
                return shape;
            static const char* const viRequired[] = {"sample_format", "channel_count", "channel_mask", "sample_rate"};
            static const char* const pcmRequired[] = {
                "sample_format", "channel_count", "channel_mask", "sample_rate", "samples_per_frame"};
            const char* const* required = profile == Profile::ViMeasurement ? viRequired : pcmRequired;
            const std::size_t requiredCount = profile == Profile::ViMeasurement ? 4 : 5;
            if (!HasExactFields(document, parameters, required, requiredCount, nullptr, 0))
                return ErrorCode::UnsupportedParameters;

            const Node* format = Find(document, parameters, "sample_format");
            const Node* channelCount = Find(document, parameters, "channel_count");
            const Node* channelMask = Find(document, parameters, "channel_mask");
            const Node* rate = Find(document, parameters, "sample_rate");
            const Node* numerator = Find(document, *rate, "numerator");
            const Node* denominator = Find(document, *rate, "denominator");
            if (profile == Profile::ViMeasurement)
            {
                const bool rateValid = (numerator->unsignedValue == 0) == (denominator->unsignedValue == 0);
                if (!StringEquals(document, *format, "vi-f32le") || channelCount->unsignedValue != 2 ||
                    channelMask->unsignedValue != 3 || !rateValid)
                    return ErrorCode::UnsupportedParameters;
            }
            else
            {
                const Node* samplesPerFrame = Find(document, parameters, "samples_per_frame");
                if (!StringEquals(document, *format, "pcm-s16le-interleaved") || channelCount->unsignedValue != 1 ||
                    channelMask->unsignedValue != 1 || numerator->unsignedValue != 16000 || denominator->unsignedValue != 1 ||
                    samplesPerFrame->unsignedValue != 256)
                    return ErrorCode::UnsupportedParameters;
            }
            return ErrorCode::None;
        }

        ErrorCode ValidateDocument(const Document& document, ClientMessage& output)
        {
            output = {};
            const Node& root = document.nodes[document.root];
            if (root.type != NodeType::Object)
                return ErrorCode::InvalidMessage;
            const Node* type = Find(document, root, "type");
            if (type == nullptr || type->type != NodeType::String)
                return ErrorCode::InvalidMessage;

            if (StringEquals(document, *type, "hello"))
            {
                static const char* const required[] = {"type", "protocol", "versions"};
                static const char* const optional[] = {"client_name", "authentication"};
                if (!HasExactFields(document, root, required, 3, optional, 2))
                    return ErrorCode::InvalidMessage;
                const Node* protocol = Find(document, root, "protocol");
                const Node* versions = Find(document, root, "versions");
                if (protocol == nullptr || !StringEquals(document, *protocol, "d2b-stream") || versions == nullptr ||
                    versions->type != NodeType::Array || versions->count < 1 || versions->count > 16)
                    return ErrorCode::InvalidMessage;
                bool supportsVersion = false;
                for (std::uint16_t item = versions->first; item != kNoIndex; item = document.items[item].next)
                {
                    const Node& version = document.nodes[document.items[item].value];
                    if (!IsVersion(document, version))
                        return ErrorCode::InvalidMessage;
                    if (StringEquals(document, version, "0.1"))
                        supportsVersion = true;
                    for (std::uint16_t prior = versions->first; prior != item; prior = document.items[prior].next)
                    {
                        const Node& priorVersion = document.nodes[document.items[prior].value];
                        if (version.text.bytes == priorVersion.text.bytes &&
                            std::memcmp(StringValue(document, version), StringValue(document, priorVersion), version.text.bytes) == 0)
                            return ErrorCode::InvalidMessage;
                    }
                }
                const Node* clientName = Find(document, root, "client_name");
                if (clientName != nullptr && !IsString(clientName, 1, 128))
                    return ErrorCode::InvalidMessage;
                const Node* authentication = Find(document, root, "authentication");
                if (authentication != nullptr)
                {
                    static const char* const authRequired[] = {"scheme", "token"};
                    const Node* scheme = Find(document, *authentication, "scheme");
                    const Node* token = Find(document, *authentication, "token");
                    if (!HasExactFields(document, *authentication, authRequired, 2, nullptr, 0) || scheme == nullptr ||
                        !StringEquals(document, *scheme, "pairing-token") || !IsString(token, 1, 256) || token->text.bytes > 256)
                        return ErrorCode::InvalidMessage;
                }
                output.type = ClientMessageType::Hello;
                output.supportsVersion01 = supportsVersion;
                return ErrorCode::None;
            }

            if (StringEquals(document, *type, "start_stream"))
            {
                static const char* const required[] = {"type", "stream", "profile", "parameters"};
                static const char* const optional[] = {"options"};
                if (!HasExactFields(document, root, required, 4, optional, 1))
                    return ErrorCode::InvalidMessage;
                const Node* stream = Find(document, root, "stream");
                const Node* profile = Find(document, root, "profile");
                const Node* parameters = Find(document, root, "parameters");
                if (!IsIdentifier(document, stream) || !IsIdentifier(document, profile) || parameters == nullptr)
                    return ErrorCode::InvalidMessage;
                const Node* options = Find(document, root, "options");
                if (options != nullptr && (options->type != NodeType::Object || options->count > 32))
                    return ErrorCode::InvalidMessage;
                const ErrorCode shape = ValidateParameterShape(document, *parameters);
                if (shape != ErrorCode::None)
                    return shape;
                Profile parsedProfile;
                if (StringEquals(document, *profile, "vi-measurement"))
                    parsedProfile = Profile::ViMeasurement;
                else if (StringEquals(document, *profile, "pcm-audio"))
                    parsedProfile = Profile::PcmAudio;
                else
                    return ErrorCode::UnsupportedProfile;
                const ErrorCode parametersResult = ValidateParameters(document, *parameters, parsedProfile);
                if (parametersResult != ErrorCode::None)
                    return parametersResult;
                output.type = ClientMessageType::StartStream;
                output.profile = parsedProfile;
                if (!CopyString(document, *stream, output.stream, sizeof(output.stream)))
                    return ErrorCode::InvalidMessage;
                return ErrorCode::None;
            }

            if (StringEquals(document, *type, "stop_stream"))
            {
                static const char* const required[] = {"type"};
                static const char* const optional[] = {"stream_id", "reason"};
                if (!HasExactFields(document, root, required, 1, optional, 2))
                    return ErrorCode::InvalidMessage;
                const Node* streamId = Find(document, root, "stream_id");
                const Node* reason = Find(document, root, "reason");
                if (streamId != nullptr && !IsInteger(streamId, 1, 0xffffffffULL))
                    return ErrorCode::InvalidMessage;
                if (reason != nullptr && !IsString(reason, 1, 256))
                    return ErrorCode::InvalidMessage;
                output.type = ClientMessageType::StopStream;
                output.hasStreamId = streamId != nullptr;
                output.streamId = streamId == nullptr ? 0 : static_cast<std::uint32_t>(streamId->unsignedValue);
                return ErrorCode::None;
            }

            if (StringEquals(document, *type, "ping"))
            {
                static const char* const required[] = {"type", "correlation"};
                const Node* correlation = Find(document, root, "correlation");
                if (!HasExactFields(document, root, required, 2, nullptr, 0) || !IsString(correlation, 1, 128) ||
                    !CopyString(document, *correlation, output.correlation, sizeof(output.correlation)))
                    return ErrorCode::InvalidMessage;
                output.type = ClientMessageType::Ping;
                output.correlationBytes = correlation->text.bytes;
                return ErrorCode::None;
            }

            return ErrorCode::InvalidMessage;
        }
    } // namespace

    const char* ErrorCodeName(ErrorCode code)
    {
        switch (code)
        {
            case ErrorCode::Busy: return "busy";
            case ErrorCode::Unauthorized: return "unauthorized";
            case ErrorCode::UnknownStream: return "unknown_stream";
            case ErrorCode::UnsupportedVersion: return "unsupported_version";
            case ErrorCode::UnsupportedProfile: return "unsupported_profile";
            case ErrorCode::UnsupportedParameters: return "unsupported_parameters";
            case ErrorCode::InvalidMessage: return "invalid_message";
            case ErrorCode::InvalidState: return "invalid_state";
            case ErrorCode::FrameTooLarge: return "frame_too_large";
            case ErrorCode::InternalError: return "internal_error";
            case ErrorCode::None: return "";
        }
        return "internal_error";
    }

    ErrorCode ParseClientMessageInto(const std::uint8_t* data, std::size_t size, ParseResult& output)
    {
        output = {};
        // The parser document is fixed-capacity BSS workspace. Product access is
        // serialized by the single HTTPD-task Transport owner; this API is
        // deliberately non-reentrant and performs no dynamic allocation.
        static Parser parser;
        static Document document;
        const ErrorCode parseResult = parser.parse(data, size, document);
        if (parseResult != ErrorCode::None)
        {
            output.error = parseResult;
            return parseResult;
        }
        output.error = ValidateDocument(document, output.message);
        return output.error;
    }

    ParseResult ParseClientMessage(const std::uint8_t* data, std::size_t size)
    {
        ParseResult result = {};
        (void)ParseClientMessageInto(data, size, result);
        return result;
    }

    ErrorCode ValidateClientMessageState(const ClientMessage& message, ControlState state, bool ownsStream)
    {
        bool allowed = false;
        switch (state)
        {
            case ControlState::Connected: allowed = message.type == ClientMessageType::Hello; break;
            case ControlState::Ready:
                allowed = message.type == ClientMessageType::StartStream || message.type == ClientMessageType::Ping;
                break;
            case ControlState::Streaming:
                allowed = message.type == ClientMessageType::StopStream || message.type == ClientMessageType::Ping;
                break;
            case ControlState::Closed: allowed = false; break;
        }
        if (!allowed || (state == ControlState::Streaming && message.type == ClientMessageType::StopStream && !ownsStream))
            return ErrorCode::InvalidState;
        return ErrorCode::None;
    }
} // namespace D2B
