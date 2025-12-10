// Copyright (c) 2025 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <random.h>
#include <util/strencodings.h>

#include <string>

namespace {

const std::string& Payload()
{
    static const std::string payload = [] {
        FastRandomContext rng(/*fDeterministic=*/true);
        std::string data;
        data.reserve(256 * 1024);
        for (size_t i = 0; i < 256 * 1024; ++i) {
            data.push_back(static_cast<char>(rng.randbits(8)));
        }
        return data;
    }();
    return payload;
}

} // namespace

static void SanitizeStringBench(benchmark::Bench& bench)
{
    const auto& payload = Payload();
    bench.batch(payload.size()).unit("byte").run([&] {
        auto sanitized = SanitizeString(payload);
        ankerl::nanobench::doNotOptimizeAway(sanitized);
    });
}

BENCHMARK(SanitizeStringBench, benchmark::PriorityLevel::LOW);
