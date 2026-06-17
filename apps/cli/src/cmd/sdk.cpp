/// @file
/// @brief Implementation of `euxis sdk-demo`.
///
/// Drives AgentLoopHarness through a configurable number of mock turns,
/// builds a UsageRecord per turn, aggregates them via libs/metrics, and
/// prints the resulting insights. Proves the Tier 1+2 primitives wire
/// together end-to-end without requiring real LLM credentials.

#include "euxis/cli/cmd/sdk.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/terminal.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "euxis/metrics/insights.hpp"
#include "euxis/runtime/agent_loop.hpp"
#include "euxis/runtime/session_store.hpp"

namespace euxis::cli::cmd {

namespace {

namespace term = terminal;
using euxis::cli::i18n::tr;

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
    sm.agent_id = "sdk-demo";
    return euxis::runtime::ConversationMessage{sm};
}

} // namespace

int cmd_sdk_demo(Context& ctx, const std::vector<std::string>& args) {
    // Argument parsing.
    std::size_t turns       = 3;
    std::string provider    = "anthropic";
    std::string model       = "claude-haiku-4-5";
    std::string session_id  = "sdk-demo-session";
    std::string agent_id    = "sdk-demo";
    bool        emit_json   = ctx.json_output;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == "--help" || a == "-h") {
            std::cerr
                << tr("Drive AgentLoopHarness through N mock turns and emit insights.")
                << "\n\n"
                << tr("Usage:") << "\n"
                << "  euxis sdk-demo [options]\n\n"
                << tr("Options:") << "\n"
                << "  --turns <N>        " << tr("Number of mock turns to run (default 3)") << "\n"
                << "  --provider <id>    " << tr("Provider id for insight pricing (default anthropic)") << "\n"
                << "  --model <id>       " << tr("Model id for insight pricing (default claude-haiku-4-5)") << "\n"
                << "  --session <id>     " << tr("Session id used in insights (default sdk-demo-session)") << "\n"
                << "  --json             " << tr("Emit insights JSON to stdout") << "\n";
            return 0;
        }
        if (a == "--turns" && i + 1 < args.size()) {
            try { turns = static_cast<std::size_t>(std::stoul(args[++i])); }
            catch (const std::exception&) {
                std::cerr << tr("error: --turns must be a positive integer\n");
                return 2;
            }
            continue;
        }
        if (a == "--provider" && i + 1 < args.size()) { provider   = args[++i]; continue; }
        if (a == "--model"    && i + 1 < args.size()) { model      = args[++i]; continue; }
        if (a == "--session"  && i + 1 < args.size()) { session_id = args[++i]; continue; }
        if (a == "--json") { emit_json = true; continue; }
    }

    if (turns == 0) {
        std::cerr << tr("error: --turns must be at least 1\n");
        return 2;
    }

    // Bring up the harness with a budget exactly large enough to cover the
    // requested turn count. Window is small so the compaction recommendation
    // surfaces after a few turns of synthetic 800-byte prompts.
    euxis::runtime::AgentLoopHarness::Config cfg;
    cfg.session_id           = session_id;
    cfg.agent_id             = agent_id;
    cfg.iteration_budget_max = turns;
    cfg.context_window       = 4096;
    euxis::runtime::AgentLoopHarness harness{std::move(cfg)};

    // Seed the conversation with a system + first user message.
    {
        euxis::runtime::SessionMessage sys{};
        sys.role     = euxis::runtime::Role::System;
        sys.content  = "you are a compliance audit agent";
        sys.agent_id = agent_id;
        harness.add_message(euxis::runtime::ConversationMessage{sys});
    }
    harness.add_message(make_user_message("scan the repo for unsigned commits"));

    auto pricing = euxis::metrics::lookup_pricing(provider, model);

    std::vector<euxis::metrics::UsageRecord> records;
    records.reserve(turns);

    std::size_t compaction_recommendations = 0;
    std::size_t successful_turns           = 0;
    std::size_t budget_blocked             = 0;

    for (std::size_t turn = 0; turn < turns; ++turn) {
        // Synthetic per-turn growth so the context engine can fire mid-run.
        harness.add_message(make_user_message(std::string(800, 'x')));

        auto res = harness.run_turn([turn]() {
            euxis::runtime::ProviderResponse r{};
            r.success       = true;
            r.output        = "synthesised response " + std::to_string(turn);
            // Synthetic but plausible token counts.
            r.input_tokens  = 200 + static_cast<int>(turn) * 50;
            r.output_tokens = 80  + static_cast<int>(turn) * 20;
            r.duration_ms   = 12.5;
            return r;
        });

        if (!res.budget_consumed) { ++budget_blocked; continue; }
        if (res.provider_response.success) ++successful_turns;
        if (res.compaction_recommended)    ++compaction_recommendations;

        euxis::metrics::UsageRecord rec{};
        rec.session_id          = session_id;
        rec.agent_id            = agent_id;
        rec.provider            = provider;
        rec.model               = model;
        rec.input_tokens        =
            static_cast<std::size_t>(res.provider_response.input_tokens);
        rec.output_tokens       =
            static_cast<std::size_t>(res.provider_response.output_tokens);
        rec.timestamp_unix_ms   = now_unix_ms();
        rec.cost_usd            = euxis::metrics::estimate_cost(
            pricing,
            rec.input_tokens,
            /*cached_input_tokens=*/0,
            rec.output_tokens);

        records.push_back(std::move(rec));
    }

    auto insights = euxis::metrics::aggregate(session_id, records);

    if (emit_json) {
        nlohmann::json out;
        out["session_id"]              = insights.session_id;
        out["turns_run"]                = successful_turns;
        out["budget_blocked"]           = budget_blocked;
        out["compaction_recommendations"] = compaction_recommendations;
        out["total_calls"]              = insights.total_calls;
        out["total_input_tokens"]        = insights.total_input_tokens;
        out["total_output_tokens"]       = insights.total_output_tokens;
        out["total_cost_usd"]            = insights.total_cost_usd;
        out["pricing_known"]             = pricing.is_known();
        out["per_model_cost"]            = nlohmann::json::array();
        for (const auto& [k, v] : insights.per_model_cost) {
            out["per_model_cost"].push_back({{"model", k}, {"cost_usd", v}});
        }
        std::cout << out.dump(2) << "\n";
        return 0;
    }

    std::cout << term::bold(tr("Euxis SDK Demo")) << "\n";
    std::cout << tr("Session:") << " " << insights.session_id << "\n";
    std::cout << tr("Turns run:") << " " << successful_turns
              << " / "            << turns << "\n";
    std::cout << tr("Budget blocked:") << " " << budget_blocked << "\n";
    std::cout << tr("Compaction recommendations:") << " "
              << compaction_recommendations << "\n";
    std::cout << tr("Total input tokens:")  << "  "
              << insights.total_input_tokens  << "\n";
    std::cout << tr("Total output tokens:") << " "
              << insights.total_output_tokens << "\n";
    if (pricing.is_known()) {
        std::cout << tr("Estimated cost:") << " $"
                  << insights.total_cost_usd << " "
                  << "(" << provider << ":" << model << ")\n";
    } else {
        std::cout << tr("Estimated cost:") << " "
                  << tr("(unknown pricing for") << " " << provider << ":"
                  << model << ")\n";
    }
    if (!insights.per_model_cost.empty()) {
        std::cout << term::bold(tr("Per-model breakdown:")) << "\n";
        for (const auto& [k, v] : insights.per_model_cost) {
            std::cout << "  " << k << ": $" << v << "\n";
        }
    }
    return 0;
}

} // namespace euxis::cli::cmd
