#include "euxis/bench/runner.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <format>
#include <numeric>
#include <vector>

#include "euxis/crypto/aes_gcm.hpp"
#include "euxis/crypto/key_derivation.hpp"
#include "euxis/a2a/agent_card.hpp"

#include <spdlog/spdlog.h>

namespace euxis::bench {

// ---------------------------------------------------------------------------
// Benchmark: crypto_throughput
// ---------------------------------------------------------------------------
static auto bench_crypto_throughput() -> BenchmarkResult {
    BenchmarkResult result;
    result.name = "crypto_throughput";
    result.suite = "performance";
    result.unit = "ops/sec";
    result.target = 50'000.0;

    // Generate a 32-byte key and 1KB of plaintext data.
    auto key_vec = crypto::generate_key(32);
    std::array<std::byte, 32> key{};
    std::copy_n(key_vec.begin(), 32, key.begin());

    std::vector<std::byte> plaintext(1024);
    for (size_t i = 0; i < plaintext.size(); ++i) {
        plaintext[i] = static_cast<std::byte>(i & 0xFF);
    }

    // Encrypt/decrypt repeatedly for 1 second.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    size_t ops = 0;
    const auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() < deadline) {
        auto enc_result = crypto::encrypt(plaintext, key);
        if (!enc_result.has_value()) {
            break;
        }

        auto dec_result = crypto::decrypt(
            enc_result->ciphertext, key, enc_result->iv);
        if (!dec_result.has_value()) {
            break;
        }

        ++ops;
    }

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double elapsed_sec =
        static_cast<double>(elapsed_us.count()) / 1'000'000.0;

    result.duration = elapsed_us;
    result.value = static_cast<double>(ops) / elapsed_sec;
    result.passed = result.value >= result.target;
    result.message = std::format(
        "{} encrypt/decrypt ops in {:.3f}s = {:.0f} ops/sec",
        ops, elapsed_sec, result.value);

    spdlog::info("performance/crypto_throughput: {}", result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Benchmark: key_derivation_p95
// ---------------------------------------------------------------------------
static auto bench_key_derivation_p95() -> BenchmarkResult {
    BenchmarkResult result;
    result.name = "key_derivation_p95";
    result.suite = "performance";
    result.unit = "ms";
    result.target = 5.0;

    const std::string password_str = "benchmark-password-2026";
    std::vector<std::byte> password(password_str.size());
    for (size_t i = 0; i < password_str.size(); ++i) {
        password[i] = static_cast<std::byte>(password_str[i]);
    }

    // Use a fixed 16-byte salt for reproducibility.
    std::vector<std::byte> salt(16);
    for (size_t i = 0; i < salt.size(); ++i) {
        salt[i] = static_cast<std::byte>(i + 1);
    }

    constexpr size_t num_runs = 100;
    std::vector<double> latencies_ms;
    latencies_ms.reserve(num_runs);

    const auto bench_start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_runs; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        // Use 0 iterations to test the session-key FastPath (BLAKE2b).
        auto dk = crypto::derive_key(password, salt, 0, 32);
        const auto t1 = std::chrono::steady_clock::now();

        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        latencies_ms.push_back(ms);
    }

    const auto bench_end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::microseconds>(
        bench_end - bench_start);

    // Compute P95 latency.
    std::sort(latencies_ms.begin(), latencies_ms.end());
    const size_t p95_index = static_cast<size_t>(
        static_cast<double>(num_runs) * 0.95) - 1;
    const double p95 = latencies_ms[p95_index];

    result.value = p95;
    result.passed = result.value <= result.target;
    result.message = std::format(
        "P95 key derivation latency: {:.3f}ms (target <={:.1f}ms)",
        p95, result.target);

    spdlog::info("performance/key_derivation_p95: {}", result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Benchmark: serialization_speed
// ---------------------------------------------------------------------------
static auto bench_serialization_speed() -> BenchmarkResult {
    BenchmarkResult result;
    result.name = "serialization_speed";
    result.suite = "performance";
    result.unit = "us/op";
    result.target = 1.0;

    constexpr size_t num_cards = 100'000;

    // Build a template AgentCard.
    a2a::AgentCard card;
    card.name = "bench-agent";
    card.description = "bench card";
    card.url = "bench.euxis.io";
    card.version = "1.0.0";
    card.capabilities.push_back(a2a::Capability{
        .name = "benchmark",
        .description = "Runs benchmarks",
        .input_schema = std::nullopt,
        .output_schema = std::nullopt,
    });

    // Warmup — populate allocator thread-local caches.
    for (size_t i = 0; i < 1'000; ++i) {
        auto d = card.to_msgpack();
        auto r = a2a::AgentCard::from_msgpack(d);
    }

    const auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_cards; ++i) {
        auto data = card.to_msgpack();
        // Verify non-empty to prevent dead-code elimination.
        if (data.empty()) [[unlikely]] __builtin_unreachable();
    }

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    result.duration = elapsed_us;
    const double avg_us =
        static_cast<double>(elapsed_us.count()) / static_cast<double>(num_cards);
    result.value = avg_us;
    result.passed = result.value <= result.target;
    result.message = std::format(
        "{} serialize/deserialize cycles in {}us, avg {:.4f}us/op",
        num_cards, elapsed_us.count(), avg_us);

    spdlog::info("performance/serialization_speed: {}", result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
void register_performance_benchmarks(BenchmarkRunner& runner) {
    runner.register_benchmark("performance", "crypto_throughput",
                              bench_crypto_throughput);
    runner.register_benchmark("performance", "key_derivation_p95",
                              bench_key_derivation_p95);
    runner.register_benchmark("performance", "serialization_speed",
                              bench_serialization_speed);
}

} // namespace euxis::bench
