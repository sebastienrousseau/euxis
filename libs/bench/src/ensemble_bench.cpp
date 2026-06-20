/// @file
/// @brief Google Benchmark suite for libs/ensemble::verify quorum.
///
/// Closes the remaining slice of audit gap #2 (the agentic gbench commit
/// covered context engine + iteration budget + classifier + tool
/// dispatch, but not the ensemble runner). A bencher.dev trend on these
/// numbers tells us whether a verifier-pool refactor preserved the
/// per-finding latency budget.
///
/// What this binary measures
///   1. BM_Ensemble_Verify_1Verifier   findings ∈ {16, 64, 256, 1024}
///   2. BM_Ensemble_Verify_2Verifiers  same Range
///   3. BM_Ensemble_Verify_3Verifiers  same Range
///   4. BM_Ensemble_Verify_RequireUnanimous_3V — exercises the
///      require_unanimous branch of update_confidence.
///
/// A deterministic mock Verifier votes true_positive 50% of the time
/// based on a hash of the rule_id — gives the quorum logic real branch
/// coverage without involving an LLM.
///
/// Run:
///   ./build/cmake-build/libs/bench/euxis_ensemble_gbench
///   ./build/cmake-build/libs/bench/euxis_ensemble_gbench --benchmark_format=json
///       --benchmark_out=ensemble_bench.json
///
/// Built only when `-DEUXIS_BUILD_GBENCH=ON` is passed.

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "euxis/ensemble/runner.hpp"
#include "euxis/ensemble/verifier.hpp"
#include "euxis/security/finding.hpp"

namespace euxis::ensemble {
namespace {

// ---------------------------------------------------------------------------
// MockVerifier — deterministic 50%-true-positive vote based on a
// trivial hash of the rule_id, so different findings get different
// votes and the quorum logic actually has to make decisions.
// ---------------------------------------------------------------------------
class MockVerifier final : public Verifier {
public:
    explicit MockVerifier(std::string id, std::uint64_t mix)
        : id_{std::move(id)}, mix_{mix} {}

    [[nodiscard]] auto provider_id() const noexcept -> std::string override {
        return id_;
    }

    [[nodiscard]] auto vote(const VoteRequest& req) -> VoteOutcome override {
        VoteOutcome out;
        out.vote.provider = id_;
        // FNV-1a-ish over rule_id + mix_ to spread votes deterministically.
        std::uint64_t h = 1469598103934665603ull ^ mix_;
        for (char c : req.rule_id) {
            h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
            h *= 1099511628211ull;
        }
        out.vote.true_positive = (h & 1ull) != 0;
        out.vote.confidence    = out.vote.true_positive ? 0.85 : 0.40;
        out.rationale          = "mock";
        return out;
    }

private:
    std::string  id_;
    std::uint64_t mix_;
};

// ---------------------------------------------------------------------------
// Build N findings. Each gets a distinct rule_id so the hash spread
// from MockVerifier produces a realistic mix of true/false votes.
// ---------------------------------------------------------------------------
auto build_findings(std::size_t n) -> std::vector<euxis::security::Finding> {
    std::vector<euxis::security::Finding> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        euxis::security::Finding f;
        f.stable_fingerprint = "fp-" + std::to_string(i);
        f.rule_id            = "bench/rule-" + std::to_string(i);
        f.message            = "synthetic finding " + std::to_string(i);
        f.severity           = euxis::security::Severity::Medium;
        f.confidence         = euxis::security::Confidence::Probable;
        f.primary_location.path         = "src/file.cpp";
        f.primary_location.start_line   = static_cast<int>(i % 1000) + 1;
        f.primary_location.start_column = 1;
        f.primary_location.snippet      = "auto x = foo();";
        out.push_back(std::move(f));
    }
    return out;
}

auto build_verifiers(std::size_t count) -> std::vector<VerifierPtr> {
    std::vector<VerifierPtr> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(std::make_shared<MockVerifier>(
            "mock-" + std::to_string(i),
            /*mix=*/static_cast<std::uint64_t>(i) * 0xdeadbeefull + 1));
    }
    return out;
}

void run_verify_bench(benchmark::State& state, std::size_t verifier_count,
                      bool require_unanimous = false) {
    const auto n = static_cast<std::size_t>(state.range(0));
    const auto verifiers = build_verifiers(verifier_count);
    EnsembleConfig cfg;
    cfg.quorum            = static_cast<int>((verifier_count + 1) / 2);
    cfg.require_unanimous = require_unanimous;

    for (auto _ : state) {
        state.PauseTiming();
        auto findings = build_findings(n);
        state.ResumeTiming();

        auto result = verify(std::move(findings), verifiers, cfg);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * n));
}

// ---------------------------------------------------------------------------
// 1, 2, 3 verifiers across a 64x finding range
// ---------------------------------------------------------------------------
void BM_Ensemble_Verify_1Verifier(benchmark::State& state) {
    run_verify_bench(state, 1);
}
BENCHMARK(BM_Ensemble_Verify_1Verifier)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond);

void BM_Ensemble_Verify_2Verifiers(benchmark::State& state) {
    run_verify_bench(state, 2);
}
BENCHMARK(BM_Ensemble_Verify_2Verifiers)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond);

void BM_Ensemble_Verify_3Verifiers(benchmark::State& state) {
    run_verify_bench(state, 3);
}
BENCHMARK(BM_Ensemble_Verify_3Verifiers)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// require_unanimous branch — different code path inside the runner
// ---------------------------------------------------------------------------
void BM_Ensemble_Verify_RequireUnanimous_3V(benchmark::State& state) {
    run_verify_bench(state, 3, /*require_unanimous=*/true);
}
BENCHMARK(BM_Ensemble_Verify_RequireUnanimous_3V)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->Unit(benchmark::kMicrosecond);

} // namespace
} // namespace euxis::ensemble

BENCHMARK_MAIN();
