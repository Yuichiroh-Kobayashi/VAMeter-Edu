#include "origin_admission.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace OA = ORIGIN_ADMISSION;

namespace
{
    struct Outcome
    {
        OA::Decision decision;
        OA::RejectReason reason;
    };

    std::uint32_t Next(std::uint32_t& state)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    Outcome Scan(const std::string& input, const OA::Policy& policy, std::uint32_t seed, bool chunked)
    {
        OA::State state;
        if (!OA::Initialize(state, policy))
            std::exit(2);
        OA::Result result = {OA::Decision::NeedMore, OA::RejectReason::None, 0, 0, 0, 0, 0};
        std::size_t offset = 0;
        while (offset < input.size())
        {
            std::size_t count = input.size() - offset;
            if (chunked)
                count = 1 + Next(seed) % (count < 23 ? count : 23);
            result = OA::Consume(state, reinterpret_cast<const std::uint8_t*>(input.data() + offset), count);
            if (result.decision == OA::Decision::Rejected || result.decision == OA::Decision::AcceptedWebSocket)
                break;
            offset += count;
        }
        return Outcome{result.decision, result.reason};
    }

    std::string Mutate(const std::string& canonical, std::uint32_t& seed, std::uint32_t mode)
    {
        std::string value = canonical;
        const std::size_t position = Next(seed) % value.size();
        switch (mode % 8)
        {
        case 0:
            value[position] = static_cast<char>(Next(seed) & 0xff);
            break;
        case 1:
            value.erase(position, 1);
            break;
        case 2:
            value.insert(position, 1, static_cast<char>(Next(seed) & 0xff));
            break;
        case 3:
            value.insert(value.find("\r\n\r\n"), "Origin: http://evil\r\n");
            break;
        case 4:
            value[position] = value[position] == '\r' ? '\n' : '\r';
            break;
        case 5:
            if (value[position] >= 'a' && value[position] <= 'z')
                value[position] -= 'a' - 'A';
            else if (value[position] >= 'A' && value[position] <= 'Z')
                value[position] += 'a' - 'A';
            else
                value[position] = '_';
            break;
        case 6:
            value[position] = value[position] == ':' ? ';' : ':';
            break;
        case 7:
            value.insert(position, "\r\n");
            break;
        }
        return value;
    }
} // namespace

int main()
{
    const std::string origin = "http://192.168.4.1";
    const OA::Policy policy = {origin.data(), origin.size()};
    const std::string inputs[] = {
        "GET /d2b/v0/stream HTTP/1.1\r\nHost: 192.168.4.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nOrigin: " + origin +
            "\r\n\r\n",
        "GET /viewer/ HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n",
    };
    std::uint32_t seed = 0x6f317278U;
    const std::uint32_t iterations = 12000;
    for (std::uint32_t index = 0; index < iterations; ++index)
    {
        const std::string mutated = Mutate(inputs[index & 1U], seed, index);
        const Outcome single = Scan(mutated, policy, seed, false);
        const std::uint32_t partition_seed = Next(seed);
        const Outcome partitioned = Scan(mutated, policy, partition_seed, true);
        const Outcome repeated = Scan(mutated, policy, partition_seed, true);
        if (single.decision != partitioned.decision || single.reason != partitioned.reason ||
            partitioned.decision != repeated.decision || partitioned.reason != repeated.reason)
        {
            std::cerr << "FAIL nondeterministic input=" << index << '\n';
            return 1;
        }
    }
    std::cout << "origin_admission_fuzz PASS seed=0x6f317278 iterations=" << iterations
              << " crash=0 hang=0 out_of_bounds=0 nondeterministic=0\n";
    return 0;
}
