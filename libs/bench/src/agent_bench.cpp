/// @file
/// @brief Google Benchmark suite for the libs/runtime agentic core.
///
/// Closes the gap identified by the 2026 ecosystem audit: the existing
/// gbench targets (euxis_perf_gbench, euxis_scan_gbench) measure crypto
/// primitives and the SBOM/sign pipeline, but the headline-feature
/// surfaces — context engine, iteration budget, approval classifier,
/// tool dispatch — had zero microbenchmark coverage. A trend on these
/// numbers lets a bencher.dev dashboard catch regressions in the
/// runtime hot paths the moment a refactor lands.
///
/// What this binary measures.
///   1. estimate_tokens                   — Range(16..4096) message vec
///   2. WindowedContextEngine::plan       — both below- and above-trigger
///   3. IterationBudget::try_consume      — uncontended + 1..8 threads
///   4. classify_approval                 — readonly / exec_capable /
///                                          control_plane (scope path)
///   5. dispatch_tool_calls round-trip    — classify + invoke + serialise
///                                          + harness::add_message
///
/// Run:
///   ./build/cmake-build/libs/bench/euxis_agent_gbench
///   ./build/cmake-build/libs/bench/euxis_agent_gbench --benchmark_format=json
///       --benchmark_out=agent_bench.json
///
/// Built only when `-DEUXIS_BUILD_GBENCH=ON` is passed.

#include <atomic>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>

#include "euxis/runtime/agent_loop.hpp"
#include "euxis/runtime/agent_session.hpp"
#include "euxis/runtime/context_engine.hpp"
#include "euxis/runtime/iteration_budget.hpp"
#include "euxis/runtime/session_store.hpp"
#include "euxis/runtime/tool_manifest.hpp"

namespace euxis::runtime {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

auto build_messages(std::size_t count, std::size_t bytes_each)
    -> std::vector<ConversationMessage> {
    std::vector<ConversationMessage> out;
    out.reserve(count);
    SessionMessage tmpl{};
    tmpl.agent_id  = "bench";
    tmpl.model     = "bench-model";
    tmpl.timestamp = "2026-01-01T00:00:00Z";
    tmpl.content   = std::string(bytes_each, 'x');
    for (std::size_t i = 0; i < count; ++i) {
        tmpl.role = (i % 2 == 0) ? Role::User : Role::Assistant;
        out.emplace_back(tmpl);
    }
    return out;
}

[[nodiscard]] auto make_decl(std::string name) -> ToolDeclaration_v2 {
    ToolDeclaration_v2 d;
    d.name        = std::move(name);
    d.description = "bench";
    return d;
}

// Minimal IToolRegistry — same shape as the example in docs/examples/cpp/
// tool_calling_loop/. Reproduced here so the bench has no docs/ dependency.
class LocalRegistry final : public IToolRegistry {
public:
    void register_tool(ToolDeclaration decl, ToolHandler h) override {
        std::string n = decl.name;
        m_[std::move(n)] = {std::move(decl), std::move(h)};
    }
    auto invoke(const std::string& n, const nlohmann::json& in)
        -> std::expected<nlohmann::json, std::string> override {
        if (auto it = m_.find(n); it != m_.end()) return it->second.h(in);
        return std::unexpected("missing");
    }
    auto list_tools() const -> std::vector<ToolDeclaration> override {
        std::vector<ToolDeclaration> out;
        for (const auto& [_, e] : m_) out.push_back(e.decl);
        return out;
    }
    void remove_tool(const std::string& n) override { m_.erase(n); }
private:
    struct E { ToolDeclaration decl{}; ToolHandler h{}; };
    std::unordered_map<std::string, E> m_;
};

using ApprovalCallback = std::function<bool(ApprovalClass, std::string_view)>;

[[nodiscard]] auto dispatch_one(const nlohmann::json& call,
                                IToolRegistry& reg,
                                AgentLoopHarness& h,
                                const ApprovalCallback& approve) -> bool {
    const auto name = call.value("name", std::string{});
    const auto args = call.value("args", nlohmann::json::object());
    ToolDeclaration_v2 d; d.name = name;
    const auto klass = classify_approval(d);
    SessionMessage tm{};
    tm.role     = Role::Assistant;
    tm.agent_id = h.agent_id();
    if (!approve(klass, name)) {
        tm.content = nlohmann::json{{"tool",name},{"status","denied"}}.dump();
        h.add_message(ConversationMessage{tm});
        return false;
    }
    auto r = reg.invoke(name, args);
    // NOLINTNEXTLINE(bugprone-branch-clone) — branches differ in keys (result vs error) and source (*r vs r.error()); the parallel shape is intentional.
    if (r.has_value()) {
        tm.content = nlohmann::json{
            {"tool",name},{"status","ok"},{"result",*r}}.dump();
    } else {
        tm.content = nlohmann::json{
            {"tool",name},{"status","error"},{"error",r.error()}}.dump();
    }
    h.add_message(ConversationMessage{tm});
    return r.has_value();
}

// ---------------------------------------------------------------------------
// 1. estimate_tokens
// ---------------------------------------------------------------------------
void BM_EstimateTokens(benchmark::State& state) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto msgs  = build_messages(count, /*bytes_each=*/400);
    for (auto _ : state) {
        auto t = estimate_tokens(msgs);
        benchmark::DoNotOptimize(t);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * count));
    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations() * count * 400));
}
BENCHMARK(BM_EstimateTokens)
    ->RangeMultiplier(4)
    ->Range(16, 4096)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// 2a. WindowedContextEngine::plan — below the 0.75 trigger ratio
// ---------------------------------------------------------------------------
void BM_WindowedPlan_NoCompaction(benchmark::State& state) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto msgs  = build_messages(count, /*bytes_each=*/200);
    // Big enough context window so the trigger never fires.
    constexpr std::size_t kWindow = 1'000'000;
    WindowedContextEngine eng;
    const auto tokens = estimate_tokens(msgs);
    for (auto _ : state) {
        auto plan = eng.plan(msgs, tokens, kWindow);
        if (!plan.is_empty()) {
            state.SkipWithError("expected empty plan below trigger");
            break;
        }
        benchmark::DoNotOptimize(plan);
    }
}
BENCHMARK(BM_WindowedPlan_NoCompaction)
    ->RangeMultiplier(4)
    ->Range(16, 4096)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// 2b. WindowedContextEngine::plan — above the trigger ratio
// ---------------------------------------------------------------------------
void BM_WindowedPlan_Compacts(benchmark::State& state) {
    const auto count = static_cast<std::size_t>(state.range(0));
    const auto msgs  = build_messages(count, /*bytes_each=*/400);
    WindowedContextEngine eng;
    const auto tokens = estimate_tokens(msgs);
    // Tight window forces tokens/window >= 0.75.
    const std::size_t window = (tokens * 4) / 3 + 1;
    for (auto _ : state) {
        auto plan = eng.plan(msgs, tokens, window);
        benchmark::DoNotOptimize(plan);
        if (plan.is_empty() && msgs.size() > 12) {
            state.SkipWithError("expected non-empty plan above trigger");
            break;
        }
    }
}
BENCHMARK(BM_WindowedPlan_Compacts)
    ->RangeMultiplier(4)
    ->Range(16, 4096)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// 3a. IterationBudget::try_consume — uncontended hot path
// ---------------------------------------------------------------------------
void BM_IterationBudget_TryConsume_Uncontended(benchmark::State& state) {
    IterationBudget budget{IterationBudget::kDefaultParent};
    for (auto _ : state) {
        if (!budget.try_consume()) {
            state.PauseTiming();
            budget.reset();
            state.ResumeTiming();
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_IterationBudget_TryConsume_Uncontended)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// 3b. IterationBudget::try_consume — contended across 1..8 threads
//
// One shared budget across all threads; each thread pulls from it with CAS.
// Google Benchmark instantiates the function once per thread; we use a
// static pointer to make the shared budget visible without globals leaking
// across threads from prior runs (re-set on every state.thread_index() == 0).
// ---------------------------------------------------------------------------
void BM_IterationBudget_TryConsume_Contended(benchmark::State& state) {
    static IterationBudget* shared = nullptr;
    if (state.thread_index() == 0) {
        delete shared;
        shared = new IterationBudget{IterationBudget::kDefaultParent * 1000};
    }
    for (auto _ : state) {
        if (!shared->try_consume()) {
            // Refund instead of reset to keep the contention realistic;
            // reset would re-synchronise every thread on the same instant.
            shared->refund();
        }
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_IterationBudget_TryConsume_Contended)
    ->ThreadRange(1, 8)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// 4. classify_approval — three distinct bands
// ---------------------------------------------------------------------------
void BM_ClassifyApproval_Readonly(benchmark::State& state) {
    const auto decl = make_decl("list_files");
    for (auto _ : state) {
        auto k = classify_approval(decl);
        benchmark::DoNotOptimize(k);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_ClassifyApproval_Readonly)->Unit(benchmark::kNanosecond);

void BM_ClassifyApproval_ExecCapable(benchmark::State& state) {
    const auto decl = make_decl("write_file");
    for (auto _ : state) {
        auto k = classify_approval(decl);
        benchmark::DoNotOptimize(k);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_ClassifyApproval_ExecCapable)->Unit(benchmark::kNanosecond);

void BM_ClassifyApproval_ControlPlane(benchmark::State& state) {
    const auto decl = make_decl("list_sessions");
    const std::string_view scope = "admin:sessions";
    for (auto _ : state) {
        auto k = classify_approval(decl, scope);
        benchmark::DoNotOptimize(k);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_ClassifyApproval_ControlPlane)->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// 5. dispatch_tool_calls round-trip — classify + invoke + serialise + append
//
// Setup cost (registry build, harness construction) is paused per-iteration
// so the reported time is the dispatch hot path only. Real workloads
// amortise setup once per session; we don't want it polluting the steady-
// state number.
// ---------------------------------------------------------------------------
void BM_ToolDispatch_RoundTrip(benchmark::State& state) {
    const auto call = nlohmann::json{
        {"name", "list_files"},
        {"args", {{"path", "src/"}}}};
    const ApprovalCallback approve =
        [](ApprovalClass, std::string_view) { return true; };

    for (auto _ : state) {
        state.PauseTiming();
        LocalRegistry reg;
        reg.register_tool(
            {.name="list_files", .description="", .input_schema={}},
            [](const nlohmann::json& a)
                -> std::expected<nlohmann::json, std::string> {
                return nlohmann::json{
                    {"path", a.value("path","")},
                    {"entries", {"a","b","c"}}};
            });
        AgentLoopHarness::Config cfg;
        cfg.session_id = "b"; cfg.agent_id = "b";
        cfg.iteration_budget_max = 1'000;
        cfg.context_window = 16'384;
        AgentLoopHarness h{std::move(cfg)};
        state.ResumeTiming();

        bool ok = dispatch_one(call, reg, h, approve);
        benchmark::DoNotOptimize(ok);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_ToolDispatch_RoundTrip)->Unit(benchmark::kMicrosecond);

} // namespace
} // namespace euxis::runtime

BENCHMARK_MAIN();
