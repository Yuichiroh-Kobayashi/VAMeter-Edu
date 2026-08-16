#include "origin_admission.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace OA = ORIGIN_ADMISSION;

namespace
{
    const std::string kOrigin = "http://192.168.4.1";

    struct Outcome
    {
        OA::Decision decision;
        OA::RejectReason reason;
        std::size_t absolute_consumed;
        std::size_t tail_length;
        std::uint32_t normal_requests;
    };

    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    std::string Normal(const std::string& target = "/viewer/")
    {
        return "GET " + target + " HTTP/1.1\r\nHost: 192.168.4.1\r\nConnection: keep-alive\r\n\r\n";
    }

    std::string WebSocket(const std::string& origin = kOrigin,
                          const std::string& target = "/d2b/v0/stream",
                          const std::string& origin_name = "Origin")
    {
        return "GET " + target + " HTTP/1.1\r\nHost: 192.168.4.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
               origin_name + ": " + origin + "\r\n\r\n";
    }

    std::string Replace(std::string value, const std::string& from, const std::string& to)
    {
        const std::size_t position = value.find(from);
        Require(position != std::string::npos, "replacement source");
        value.replace(position, from.size(), to);
        return value;
    }

    Outcome
    Run(const std::string& input, const OA::Policy& policy, const std::vector<std::size_t>& chunks = std::vector<std::size_t>())
    {
        OA::State state;
        Require(OA::Initialize(state, policy), "policy initialization");
        Outcome outcome = {OA::Decision::NeedMore, OA::RejectReason::None, 0, 0, 0};
        std::size_t offset = 0;
        std::size_t chunk_index = 0;
        while (offset < input.size())
        {
            const std::size_t remaining = input.size() - offset;
            std::size_t count = chunks.empty() ? remaining : chunks[chunk_index++];
            if (count > remaining)
                count = remaining;
            const OA::Result result = OA::Consume(state, reinterpret_cast<const std::uint8_t*>(input.data() + offset), count);
            outcome.decision = result.decision;
            outcome.reason = result.reason;
            outcome.absolute_consumed = offset + result.consumed;
            outcome.tail_length = result.tail_length;
            outcome.normal_requests += result.completed_normal_requests;
            if (result.decision == OA::Decision::Rejected || result.decision == OA::Decision::AcceptedWebSocket)
                return outcome;
            offset += count;
        }
        return outcome;
    }

    Outcome Single(const std::string& input, const OA::Policy& policy) { return Run(input, policy); }

    void Expect(const std::string& input, const OA::Policy& policy, OA::Decision decision, const char* label)
    {
        Require(Single(input, policy).decision == decision, label);
    }

    void ExpectReject(const std::string& input, const OA::Policy& policy, const char* label)
    {
        Expect(input, policy, OA::Decision::Rejected, label);
    }

    void Fragmentation(const std::string& input, const OA::Policy& policy, const char* label)
    {
        const Outcome baseline = Single(input, policy);
        for (std::size_t split = 0; split <= input.size(); ++split)
        {
            std::vector<std::size_t> chunks;
            if (split != 0)
                chunks.push_back(split);
            if (split != input.size())
                chunks.push_back(input.size() - split);
            const Outcome current = chunks.empty() ? baseline : Run(input, policy, chunks);
            Require(current.decision == baseline.decision && current.reason == baseline.reason, label);
        }
        const std::vector<std::size_t> bytes(input.size(), 1);
        const Outcome bytewise = Run(input, policy, bytes);
        Require(bytewise.decision == baseline.decision && bytewise.reason == baseline.reason, label);
    }
} // namespace

int main()
{
    const OA::Policy policy = {kOrigin.data(), kOrigin.size()};
    Require(sizeof(OA::State) <= 64, "state size ceiling");

    Expect(Normal(), policy, OA::Decision::AcceptedNormalHttp, "ordinary static GET");
    Expect(Normal("/d2b/v0/capabilities"), policy, OA::Decision::AcceptedNormalHttp, "ordinary D2B GET");
    const std::string valid = WebSocket();
    Expect(valid, policy, OA::Decision::AcceptedWebSocket, "valid WebSocket");
    Expect(WebSocket(kOrigin, "/d2b/v0/stream", "oRiGiN"), policy, OA::Decision::AcceptedWebSocket, "mixed-case Origin field");

    const char* wrong_origins[] = {
        "",
        "null",
        "https://192.168.4.1",
        "http://192.168.4.2",
        "http://192.168.4.1:81",
        "http://192.168.4.",
        "http://192.168.4.1evil",
        "http://192.168.4.1/path",
    };
    for (std::size_t index = 0; index < sizeof(wrong_origins) / sizeof(wrong_origins[0]); ++index)
        ExpectReject(WebSocket(wrong_origins[index]), policy, "Origin reject matrix");
    ExpectReject(Replace(valid, "Origin: " + kOrigin, "Origin:"), policy, "empty Origin without OWS");
    ExpectReject(Replace(valid, "Origin: " + kOrigin + "\r\n", ""), policy, "missing Origin");
    ExpectReject(Replace(valid, "\r\n\r\n", "\r\nOrigin: http://evil\r\n\r\n"), policy, "duplicate valid evil");
    ExpectReject(
        Replace(valid, "Origin: " + kOrigin, "Origin: http://evil\r\nOrigin: " + kOrigin), policy, "duplicate evil valid");
    ExpectReject(Replace(valid, "\r\n\r\n", "\r\nOrigin: " + kOrigin + "\r\n\r\n"), policy, "duplicate valid valid");
    ExpectReject(WebSocket(kOrigin + ",http://evil"), policy, "compact comma");
    ExpectReject(WebSocket(kOrigin + ", http://evil"), policy, "OWS comma");

    const std::string origin191 = "http://" + std::string(184, 'a');
    const OA::Policy policy191 = {origin191.data(), origin191.size()};
    Expect(WebSocket(origin191), policy191, OA::Decision::AcceptedWebSocket, "191-byte Origin");
    ExpectReject(WebSocket(origin191 + "a"), policy191, "192-byte Origin");

    const std::string invalid_http[] = {
        Replace(Normal(), "GET", "POST"),
        Replace(Normal(), "HTTP/1.1", "HTTP/1.0"),
        "GET http://192.168.4.1/viewer/ HTTP/1.1\r\n\r\n",
        "GET  /viewer/ HTTP/1.1\r\n\r\n",
        "GET /viewer/ HTTP/1.1\nHost: x\n\n",
        "GET /viewer/ HTTP/1.1\r\n folded: x\r\n\r\n",
        "GET /viewer/ HTTP/1.1\r\nContent-Length: 0\r\n\r\n",
        "GET /viewer/ HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n",
        "GET /viewer/ HTTP/1.1\r\nBad Header: x\r\n\r\n",
    };
    for (std::size_t index = 0; index < sizeof(invalid_http) / sizeof(invalid_http[0]); ++index)
        ExpectReject(invalid_http[index], policy, "HTTP grammar matrix");
    ExpectReject(Normal("/" + std::string(1025, 'x')), policy, "overlong target");
    ExpectReject("GET / HTTP/1.1\r\nX: " + std::string(5000, 'x') + "\r\n\r\n", policy, "overlong header");

    const char* wrong_targets[] = {
        "/wrong",
        "/d2b/v0/strea",
        "/d2b/v0/streamx",
        "/d2b/v0/stream?x=1",
        "/d2b/v0/stream#x",
    };
    for (std::size_t index = 0; index < sizeof(wrong_targets) / sizeof(wrong_targets[0]); ++index)
        ExpectReject(WebSocket(kOrigin, wrong_targets[index]), policy, "WebSocket target matrix");
    ExpectReject(Replace(valid, "Upgrade: websocket\r\n", ""), policy, "missing Upgrade");
    ExpectReject(Replace(valid, "Upgrade: websocket", "Upgrade: h2c"), policy, "wrong Upgrade");
    ExpectReject(Replace(valid, "Connection: Upgrade\r\n", ""), policy, "missing Connection");
    ExpectReject(Replace(valid, "Connection: Upgrade", "Connection: keep-alive, Upgrade"), policy, "malformed Connection");
    ExpectReject(
        Replace(valid, "Upgrade: websocket\r\n", "Upgrade: websocket\r\nUpgrade: websocket\r\n"), policy, "duplicate Upgrade");
    ExpectReject(Replace(valid, "Connection: Upgrade\r\n", "Connection: Upgrade\r\nConnection: Upgrade\r\n"),
                 policy,
                 "duplicate Connection");

    Fragmentation(valid, policy, "valid fragmentation");
    Fragmentation(Replace(valid, "Origin: " + kOrigin + "\r\n", ""), policy, "missing Origin fragmentation");
    Fragmentation(WebSocket(kOrigin + ",http://evil"), policy, "comma Origin fragmentation");
    Fragmentation(Replace(valid, "Connection: Upgrade", "Connection: broken"), policy, "Connection fragmentation");
    for (std::size_t first = 0; first <= valid.size(); ++first)
    {
        for (std::size_t second = first; second <= valid.size(); ++second)
        {
            std::vector<std::size_t> chunks;
            if (first != 0)
                chunks.push_back(first);
            if (second != first)
                chunks.push_back(second - first);
            if (second != valid.size())
                chunks.push_back(valid.size() - second);
            const Outcome current = chunks.empty() ? Single(valid, policy) : Run(valid, policy, chunks);
            Require(current.decision == OA::Decision::AcceptedWebSocket, "all two-split combinations");
        }
    }

    const std::string normal = Normal();
    const std::string pipelines[] = {
        normal + normal,
        normal + valid,
        normal + WebSocket("http://evil"),
        normal + Replace(valid, "\r\n\r\n", "\r\nOrigin: " + kOrigin + "\r\n\r\n"),
        normal + normal + valid,
    };
    const OA::Decision pipeline_decisions[] = {
        OA::Decision::AcceptedNormalHttp,
        OA::Decision::AcceptedWebSocket,
        OA::Decision::Rejected,
        OA::Decision::Rejected,
        OA::Decision::AcceptedWebSocket,
    };
    for (std::size_t index = 0; index < sizeof(pipelines) / sizeof(pipelines[0]); ++index)
        Expect(pipelines[index], policy, pipeline_decisions[index], "pipeline matrix");
    const std::vector<std::size_t> boundary_chunks{normal.size(), valid.size()};
    Require(Run(normal + valid, policy, boundary_chunks).decision == OA::Decision::AcceptedWebSocket,
            "fragment at request boundary");
    const std::size_t origin_split = normal.size() + valid.find(kOrigin) + 5;
    const std::vector<std::size_t> origin_chunks{origin_split, normal.size() + valid.size() - origin_split};
    Require(Run(normal + valid, policy, origin_chunks).decision == OA::Decision::AcceptedWebSocket,
            "fragment inside second Origin");

    std::string framed = valid;
    const char binary[] = {'\0', '\n', '\r', 'G', 'E', 'T', static_cast<char>(0xff)};
    framed.append(binary, sizeof(binary));
    OA::State state;
    Require(OA::Initialize(state, policy), "tail policy initialization");
    const OA::Result framed_result = OA::Consume(state, reinterpret_cast<const std::uint8_t*>(framed.data()), framed.size());
    Require(framed_result.decision == OA::Decision::AcceptedWebSocket && framed_result.tail_offset == valid.size() &&
                framed_result.tail_length == sizeof(binary) && framed_result.request_header_bytes == valid.size(),
            "WebSocket tail position");
    Require(std::memcmp(framed.data() + framed_result.tail_offset, binary, sizeof(binary)) == 0, "WebSocket tail unchanged");
    const OA::Result opaque = OA::Consume(state, reinterpret_cast<const std::uint8_t*>(binary), sizeof(binary));
    Require(opaque.decision == OA::Decision::AcceptedWebSocket && opaque.consumed == 0 && opaque.tail_length == sizeof(binary),
            "permanent opaque state");

    std::cout << "origin_admission_test PASS state_size=" << sizeof(OA::State) << '\n';
    return 0;
}
