/// @file
/// @brief End-to-end SDK example: AgentLoopHarness + CredentialPool +
///        per-session insights, all wired together in one binary.
///
/// Companion to a2a_minimal_server. Where that example walks the A2A
/// protocol surface, this one walks the *agent-loop* surface: the
/// libs/runtime harness driving turns under a bounded iteration
/// budget, with libs/core credential-pool rotation on simulated 429s,
/// and libs/metrics aggregation producing a per-session cost report.
///
/// Build:  configure with -DEUXIS_BUILD_EXAMPLES=ON
/// Run:    ./cmake-build/docs/examples/cpp/agent_loop_demo/euxis_example_agent_loop_demo

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/core/credential_pool.hpp"
#include "euxis/metrics/insights.hpp"
#include "euxis/runtime/agent_loop.hpp"
#include "euxis/runtime/session_store.hpp"

namespace {

[[nodiscard]] auto now_unix_ms() noexcept -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

[[nodiscard]] auto make_user_message(std::string content)
    -> euxis::runtime::ConversationMessage {
    euxis::runtime::SessionMessage sm{};
    sm.role     = euxis::runtime::Role::User;
    sm.content  = std::move(content);
    sm.agent_id = "agent-loop-demo";
    return euxis::runtime::ConversationMessage{sm};
}

void print_step(std::string_view label, const nlohmann::json& payload) {
    std::cout << "\n[" << label << "]\n"
              << payload.dump(2) << '\n';
}

} // namespace

auto main() -> int {
    using namespace euxis;

    // -----------------------------------------------------------------------
    // 1. Credential pool: three "keys" available, one will go on cooldown.
    // -----------------------------------------------------------------------
    std::vector<core::Credential> creds;
    for (const auto& id : {"key-a", "key-b", "key-c"}) {
        core::Credential c;
        c.id     = id;
        c.secret = std::string{"secret-for-"} + id;
        creds.push_back(std::move(c));
    }
    core::CredentialPool pool{std::move(creds)};
    std::cout << "Credential pool: " << pool.size() << " keys, "
              << pool.available_count(now_unix_ms()) << " currently available\n";

    // -----------------------------------------------------------------------
    // 2. Agent loop harness with a 5-turn budget and an 8 KiB window.
    // -----------------------------------------------------------------------
    runtime::AgentLoopHarness::Config cfg;
    cfg.session_id           = "demo-session-001";
    cfg.agent_id             = "agent-loop-demo";
    cfg.iteration_budget_max = 5;
    cfg.context_window       = 8192;
    runtime::AgentLoopHarness harness{std::move(cfg)};

    runtime::SessionMessage system{};
    system.role     = runtime::Role::System;
    system.content  = "You are a compliance audit agent. Answer succinctly.";
    system.agent_id = "agent-loop-demo";
    harness.add_message(runtime::ConversationMessage{system});

    // -----------------------------------------------------------------------
    // 3. Provider pricing — use a real entry from the seeded table.
    // -----------------------------------------------------------------------
    constexpr std::string_view kProvider = "anthropic";
    constexpr std::string_view kModel    = "claude-haiku-4-5";
    const auto pricing = metrics::lookup_pricing(kProvider, kModel);
    std::cout << "Pricing for " << kProvider << ":" << kModel << " is "
              << (pricing.is_known() ? "known" : "UNKNOWN") << '\n';

    // -----------------------------------------------------------------------
    // 4. Drive 5 turns. Turn 2 simulates a 429: rotate the credential,
    //    cool the offending key for 60s.
    // -----------------------------------------------------------------------
    std::vector<metrics::UsageRecord> records;
    records.reserve(5);

    for (std::size_t turn = 0; turn < 5; ++turn) {
        const auto now = now_unix_ms();
        auto cred = pool.next_available(now);
        if (!cred) {
            std::cerr << "All credentials are cooled down — aborting\n";
            break;
        }

        harness.add_message(make_user_message(
            "scan service-" + std::to_string(turn) + " for unsigned commits"));

        auto turn_result = harness.run_turn([&turn]() {
            runtime::ProviderResponse r{};
            r.success       = true;
            r.output        = "service-" + std::to_string(turn) + ": clean";
            r.input_tokens  = 220 + static_cast<int>(turn) * 30;
            r.output_tokens = 60  + static_cast<int>(turn) * 15;
            r.duration_ms   = 18.0;
            return r;
        });

        if (!turn_result.budget_consumed) {
            std::cout << "Turn " << turn << ": budget exhausted, stopping\n";
            break;
        }

        // Simulate a transient 429 on turn 2: classify, rotate, cool down.
        if (turn == 2) {
            const auto reason = core::classify_failure(
                429, "rate limit exceeded; retry-after 60");
            const auto action = core::recovery_for(reason);
            std::cout << "Turn " << turn
                      << ": simulated 429 — reason=" << core::reason_name(reason)
                      << ", action=" << (action == core::RecoveryAction::Retry
                                             ? "retry"
                                             : "other")
                      << "; cooling " << *cred << " for 60s\n";
            pool.cool_down(*cred, now, /*duration_ms=*/60'000);
        }

        // Record usage for the metrics rollup.
        metrics::UsageRecord rec{};
        rec.session_id        = "demo-session-001";
        rec.agent_id          = "agent-loop-demo";
        rec.provider          = std::string{kProvider};
        rec.model             = std::string{kModel};
        rec.input_tokens      =
            static_cast<std::size_t>(turn_result.provider_response.input_tokens);
        rec.output_tokens     =
            static_cast<std::size_t>(turn_result.provider_response.output_tokens);
        rec.timestamp_unix_ms = now;
        rec.cost_usd          = metrics::estimate_cost(
            pricing, rec.input_tokens, /*cached=*/0, rec.output_tokens);
        records.push_back(std::move(rec));

        if (turn_result.compaction_recommended) {
            std::cout << "Turn " << turn
                      << ": context engine recommends compaction (range ["
                      << turn_result.compaction_plan.compact_start << ", "
                      << turn_result.compaction_plan.compact_end << "))\n";
        }
    }

    // -----------------------------------------------------------------------
    // 5. Aggregate insights and emit JSON.
    // -----------------------------------------------------------------------
    auto insights = metrics::aggregate("demo-session-001", records);
    nlohmann::json out;
    out["session_id"]           = insights.session_id;
    out["total_calls"]          = insights.total_calls;
    out["total_input_tokens"]   = insights.total_input_tokens;
    out["total_output_tokens"]  = insights.total_output_tokens;
    out["total_cost_usd"]       = insights.total_cost_usd;
    out["budget_remaining"]     = harness.budget().remaining();
    out["budget_capacity"]      = harness.budget().capacity();
    out["credentials_available_now"] =
        pool.available_count(now_unix_ms());
    out["credentials_total"]    = pool.size();
    out["per_model_cost"]       = nlohmann::json::array();
    for (const auto& [k, v] : insights.per_model_cost) {
        out["per_model_cost"].push_back({{"model", k}, {"cost_usd", v}});
    }
    print_step("agent_loop_demo summary", out);

    std::cout << "\nDemo completed successfully.\n";
    return 0;
}
