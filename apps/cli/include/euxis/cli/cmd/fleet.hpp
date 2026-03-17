/// @file
/// @brief Fleet commands
#pragma once

#include "euxis/cli/command.hpp"

#include <optional>
#include <vector>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::cli::cmd {

// Policy types exposed for reuse by surface commands
struct PolicyViolation {
    std::string gate;
    std::string detail;
};

struct Policy {
    std::string min_verdict = "CAUTION";
    int min_confidence = 0;
    double min_critical_coverage = 1.0;
    bool fail_on_regression = false;
    int max_drift = 100;
    bool fail_on_escalation = false;
    bool fail_on_budget_exceeded = false;
};

auto load_policy(const std::string& data_dir, const std::string& explicit_path = "") -> std::optional<Policy>;
auto evaluate_policy(const nlohmann::json& artifact, const Policy& policy) -> std::vector<PolicyViolation>;

int cmd_agent(Context& ctx, const std::vector<std::string>& args);
int cmd_agent_bootstrap(Context& ctx, const std::vector<std::string>& args);
int cmd_squad(Context& ctx, const std::vector<std::string>& args);
int cmd_combo(Context& ctx, const std::vector<std::string>& args);
int cmd_playbook(Context& ctx, const std::vector<std::string>& args);
int cmd_ci(Context& ctx, const std::vector<std::string>& args);
int cmd_dispatch(Context& ctx, const std::vector<std::string>& args);
int cmd_council(Context& ctx, const std::vector<std::string>& args);
int cmd_loop(Context& ctx, const std::vector<std::string>& args);
int cmd_synthesize(Context& ctx, const std::vector<std::string>& args);

} // namespace euxis::cli::cmd
