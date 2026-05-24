/// @file
/// @brief Google Benchmark port of the libs/bench performance suite.
///
/// Mirrors the four benchmarks in performance_bench.cpp 1:1 (crypto
/// throughput simple + cached, key derivation BLAKE2b fast path, agent-card
/// msgpack round-trip) so results can be cross-validated against the custom
/// harness. Built only when -DEUXIS_BUILD_GBENCH=ON is passed.
///
/// Run:
///   ./cmake-build/libs/bench/euxis_perf_gbench
///   ./cmake-build/libs/bench/euxis_perf_gbench --benchmark_format=json
///   ./cmake-build/libs/bench/euxis_perf_gbench --benchmark_filter=Crypto
///
/// JSON output is compatible with bencher.dev and the
/// benchmark-action/github-action-benchmark trend tracker.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "euxis/a2a/agent_card.hpp"
#include "euxis/crypto/aes_gcm.hpp"
#include "euxis/crypto/key_derivation.hpp"

namespace {

constexpr std::size_t kPayloadBytes = 1024;

auto make_key() -> std::array<std::byte, 32> {
    auto key_vec = euxis::crypto::generate_key(32);
    std::array<std::byte, 32> key{};
    std::copy_n(key_vec.begin(), 32, key.begin());
    return key;
}

auto make_payload() -> std::vector<std::byte> {
    std::vector<std::byte> p(kPayloadBytes);
    for (std::size_t i = 0; i < p.size(); ++i) {
        p[i] = static_cast<std::byte>(i & 0xFF);
    }
    return p;
}

auto make_card() -> euxis::a2a::AgentCard {
    euxis::a2a::AgentCard c;
    c.name = "bench-agent";
    c.description = "bench card";
    c.url = "bench.euxis.io";
    c.version = "1.0.0";
    c.capabilities.push_back(euxis::a2a::Capability{
        .name = "benchmark",
        .description = "Runs benchmarks",
        .input_schema = std::nullopt,
        .output_schema = std::nullopt,
    });
    return c;
}

// ---------------------------------------------------------------------------
// Crypto throughput — simple API (re-derives key schedule each call).
// ---------------------------------------------------------------------------
void BM_CryptoThroughput_Simple(benchmark::State& state) {
    const auto key = make_key();
    const auto plaintext = make_payload();

    for (auto _ : state) {
        auto enc = euxis::crypto::encrypt(plaintext, key);
        benchmark::DoNotOptimize(enc);
        if (!enc.has_value()) {
            state.SkipWithError("encrypt failed");
            break;
        }
        auto dec = euxis::crypto::decrypt(enc->ciphertext, key, enc->iv);
        benchmark::DoNotOptimize(dec);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations() * kPayloadBytes));
}
BENCHMARK(BM_CryptoThroughput_Simple)->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// Crypto throughput — cached AesGcmContext (key schedule amortized).
// ---------------------------------------------------------------------------
void BM_CryptoThroughput_Cached(benchmark::State& state) {
    const auto key = make_key();
    auto ctx_result = euxis::crypto::AesGcmContext::create(key);
    if (!ctx_result.has_value()) {
        state.SkipWithError("AesGcmContext::create failed");
        return;
    }
    const auto& ctx = *ctx_result;
    const auto plaintext = make_payload();

    for (auto _ : state) {
        auto enc = ctx.encrypt(plaintext);
        benchmark::DoNotOptimize(enc);
        if (!enc.has_value()) {
            state.SkipWithError("encrypt failed");
            break;
        }
        auto dec = ctx.decrypt(enc->ciphertext, enc->iv);
        benchmark::DoNotOptimize(dec);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations() * kPayloadBytes));
}
BENCHMARK(BM_CryptoThroughput_Cached)->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// Key derivation — BLAKE2b fast path (iterations=0).
// ---------------------------------------------------------------------------
void BM_KeyDerivation_FastPath(benchmark::State& state) {
    const std::string password_str = "benchmark-password-2026";
    std::vector<std::byte> password(password_str.size());
    for (std::size_t i = 0; i < password_str.size(); ++i) {
        password[i] = static_cast<std::byte>(password_str[i]);
    }
    std::vector<std::byte> salt(16);
    for (std::size_t i = 0; i < salt.size(); ++i) {
        salt[i] = static_cast<std::byte>(i + 1);
    }

    for (auto _ : state) {
        auto dk = euxis::crypto::derive_key(password, salt, /*iters=*/0, 32);
        benchmark::DoNotOptimize(dk);
    }
}
BENCHMARK(BM_KeyDerivation_FastPath)->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// Agent card msgpack round-trip.
// ---------------------------------------------------------------------------
void BM_AgentCardMsgpackRoundTrip(benchmark::State& state) {
    const auto card = make_card();

    for (auto _ : state) {
        auto data = card.to_msgpack();
        benchmark::DoNotOptimize(data);
        auto round_trip = euxis::a2a::AgentCard::from_msgpack(data);
        benchmark::DoNotOptimize(round_trip);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_AgentCardMsgpackRoundTrip)->Unit(benchmark::kNanosecond);

} // namespace

// Google Benchmark provides BENCHMARK_MAIN() — no custom main needed.
BENCHMARK_MAIN();
