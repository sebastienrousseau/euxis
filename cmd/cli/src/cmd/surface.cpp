#include "euxis/cli/cmd/surface.hpp"
#include "euxis/cli/cmd/fleet.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/terminal.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

using euxis::cli::i18n::tr;

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;
namespace term = terminal;

/// Parse common flags shared by check/triage/review.
/// Extracts target (first non-flag positional) and passthrough flags.
struct ParsedArgs {
    std::string target;
    std::vector<std::string> passthrough;
};

auto parse_common(const std::vector<std::string>& args,
                  const std::vector<std::string>& skip_flags = {}) -> ParsedArgs {
    ParsedArgs result;
    for (size_t i = 0; i < args.size(); ++i) {
        // Skip mode-like flags handled by caller
        bool skip = false;
        for (const auto& sf : skip_flags) {
            if (args[i] == sf) { skip = true; break; }
        }
        if (skip) continue;

        if (args[i] == "--policy") {
            result.passthrough.push_back("--policy");
            if (i + 1 < args.size() && !args[i + 1].starts_with("--")) {
                result.passthrough.push_back(args[++i]);
            }
        } else if (args[i] == "--ci") {
            result.passthrough.push_back("--ci");
        } else if (args[i] == "--json") {
            result.passthrough.push_back("--json");
        } else if (args[i] == "--help" || args[i] == "-h") {
            // Handled per-command
            result.passthrough.push_back("--help");
        } else if (!args[i].starts_with("--") && result.target.empty()) {
            result.target = args[i];
        }
    }
    return result;
}

/// Build playbook args: verify-everything [target] --mode <mode> [passthrough...]
auto build_playbook_args(const std::string& mode,
                         const std::string& target,
                         const std::vector<std::string>& passthrough) -> std::vector<std::string> {
    std::vector<std::string> pb_args = {"verify-everything"};
    if (!target.empty()) pb_args.push_back(target);
    pb_args.push_back("--mode");
    pb_args.push_back(mode);
    for (const auto& p : passthrough) pb_args.push_back(p);
    return pb_args;
}

} // namespace

// --- check: primary repo verification ---

int cmd_check(Context& ctx, const std::vector<std::string>& args) {
    // euxis check [target] [--triage] [--standard] [--forensic] [--policy [path]] [--ci] [--json]
    for (const auto& a : args) {
        if (a == "--help" || a == "-h") {
            std::cerr << tr("Verify a repository or target.") << "\n\n"
                      << tr("Usage:") << "\n"
                      << "  euxis check [target] [options]\n\n"
                      << tr("Options:") << "\n"
                      << "  --triage          " << tr("Run fast triage instead of standard verification") << "\n"
                      << "  --standard        " << tr("Force standard mode (default)") << "\n"
                      << "  --forensic        " << tr("Run the most exhaustive verification") << "\n"
                      << "  --policy [path]   " << tr("Apply policy evaluation") << "\n"
                      << "  --ci              " << tr("Emit CI-safe output") << "\n"
                      << "  --json            " << tr("Emit artifact JSON to stdout") << "\n";
            return 0;
        }
    }

    std::string mode = "standard";
    for (const auto& a : args) {
        if (a == "--triage")   mode = "flash";
        if (a == "--standard") mode = "standard";
        if (a == "--forensic") mode = "forensic";
    }

    auto parsed = parse_common(args, {"--triage", "--standard", "--forensic"});
    auto pb_args = build_playbook_args(mode, parsed.target, parsed.passthrough);
    return cmd_playbook(ctx, pb_args);
}

// --- triage: fast bounded screening ---

int cmd_triage(Context& ctx, const std::vector<std::string>& args) {
    // euxis triage [target] [--policy [path]] [--ci] [--json]
    for (const auto& a : args) {
        if (a == "--help" || a == "-h") {
            std::cerr << tr("Run a fast bounded triage pass.") << "\n\n"
                      << tr("Usage:") << "\n"
                      << "  euxis triage [target] [options]\n\n"
                      << tr("Options:") << "\n"
                      << "  --policy [path]   " << tr("Apply policy evaluation") << "\n"
                      << "  --ci              " << tr("Emit CI-safe output") << "\n"
                      << "  --json            " << tr("Emit artifact JSON to stdout") << "\n";
            return 0;
        }
    }

    auto parsed = parse_common(args);
    auto pb_args = build_playbook_args("flash", parsed.target, parsed.passthrough);
    return cmd_playbook(ctx, pb_args);
}

// --- review: deep verification ---

int cmd_review(Context& ctx, const std::vector<std::string>& args) {
    // euxis review [target] [--forensic] [--policy [path]] [--ci] [--json]
    for (const auto& a : args) {
        if (a == "--help" || a == "-h") {
            std::cerr << tr("Run deeper verification than triage.") << "\n\n"
                      << tr("Usage:") << "\n"
                      << "  euxis review [target] [options]\n\n"
                      << tr("Options:") << "\n"
                      << "  --forensic        " << tr("Run forensic verification") << "\n"
                      << "  --policy [path]   " << tr("Apply policy evaluation") << "\n"
                      << "  --ci              " << tr("Emit CI-safe output") << "\n"
                      << "  --json            " << tr("Emit artifact JSON to stdout") << "\n";
            return 0;
        }
    }

    std::string mode = "standard";
    for (const auto& a : args) {
        if (a == "--forensic") mode = "forensic";
    }

    auto parsed = parse_common(args, {"--forensic"});
    auto pb_args = build_playbook_args(mode, parsed.target, parsed.passthrough);
    return cmd_playbook(ctx, pb_args);
}

// --- compare: A/B comparison (flash vs standard) ---

int cmd_compare(Context& ctx, const std::vector<std::string>& args) {
    // euxis compare <target> [--json] [--policy [path]]
    for (const auto& a : args) {
        if (a == "--help" || a == "-h") {
            std::cerr << tr("Compare triage and standard verification on the same target.") << "\n\n"
                      << tr("Usage:") << "\n"
                      << "  euxis compare <target> [options]\n\n"
                      << tr("Options:") << "\n"
                      << "  --json            " << tr("Emit artifact JSON to stdout") << "\n"
                      << "  --policy [path]   " << tr("Apply policy evaluation") << "\n";
            return 0;
        }
    }

    auto parsed = parse_common(args);

    if (parsed.target.empty()) {
        std::cerr << tr("Error: compare requires an explicit target.") << "\n\n"
                  << tr("Usage:") << "  euxis compare <target>\n\n"
                  << tr("Examples:") << "\n"
                  << "  euxis compare .\n"
                  << "  euxis compare /path/to/repo\n"
                  << "  euxis compare https://github.com/org/repo\n";
        return 2;
    }

    // Build playbook --compare args, preserving target for future use
    std::vector<std::string> pb_args = {"verify-everything"};
    if (!parsed.target.empty()) pb_args.push_back(parsed.target);
    pb_args.push_back("--compare");
    for (const auto& p : parsed.passthrough) pb_args.push_back(p);

    return cmd_playbook(ctx, pb_args);
}

// --- stats: metrics and drift history ---

int cmd_stats(Context& ctx, const std::vector<std::string>& args) {
    // euxis stats [--since <date>] [--last <N>] [--json] [--check-baseline]
    for (const auto& a : args) {
        if (a == "--help" || a == "-h") {
            std::cerr << tr("Show validation metrics and drift history.") << "\n\n"
                      << tr("Usage:") << "\n"
                      << "  euxis stats [options]\n\n"
                      << tr("Options:") << "\n"
                      << "  --since <date>    " << tr("Filter history by date (YYYY-MM-DD)") << "\n"
                      << "  --last <N>        " << tr("Show only last N runs") << "\n"
                      << "  --check-baseline  " << tr("Exit non-zero if targets not met") << "\n"
                      << "  --json            " << tr("Emit JSON output") << "\n";
            return 0;
        }
    }

    std::vector<std::string> pb_args = {"--stats"};
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--since" && i + 1 < args.size()) {
            pb_args.push_back("--since");
            pb_args.push_back(args[++i]);
        } else if (args[i] == "--last" && i + 1 < args.size()) {
            pb_args.push_back("--last");
            pb_args.push_back(args[++i]);
        } else if (args[i] == "--check-baseline") {
            pb_args.push_back("--check-baseline");
        } else if (args[i] == "--json") {
            pb_args.push_back("--json");
        }
    }

    return cmd_playbook(ctx, pb_args);
}

// --- policy: inspection and enforcement ---

int cmd_policy(Context& ctx, const std::vector<std::string>& args) {
    // euxis policy <check|show|validate> [options]
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::cerr << tr("Policy inspection and enforcement.") << "\n\n"
                  << tr("Usage:") << "\n"
                  << "  euxis policy <subcommand> [options]\n\n"
                  << tr("Subcommands:") << "\n"
                  << "  show              " << tr("Display the active policy file") << "\n"
                  << "  validate [path]   " << tr("Validate policy JSON syntax") << "\n"
                  << "  check [target]    " << tr("Evaluate policy against latest or fresh verdict") << "\n";
        return args.empty() ? 2 : 0;
    }

    const auto& sub = args[0];

    // --- policy show ---
    if (sub == "show") {
        ConfigLoader loader(ctx.data_dir);
        auto policy = loader.load("config/policy.json");
        if (!policy) {
            std::cerr << tr("No policy file found at data/config/policy.json") << "\n";
            return 1;
        }
        if (ctx.json_output) {
            std::cout << policy->dump(2) << "\n";
        } else {
            std::cout << term::bold(tr("Active Policy")) << "\n";
            for (const auto& [key, val] : policy->items()) {
                std::cout << "  " << term::cyan(key) << ": " << val.dump() << "\n";
            }
        }
        return 0;
    }

    // --- policy validate ---
    if (sub == "validate") {
        std::string path;
        if (args.size() > 1 && !args[1].starts_with("--")) {
            path = args[1];
        } else {
            path = (fs::path(ctx.data_dir) / "config" / "policy.json").string();
        }

        if (!fs::exists(path)) {
            std::cerr << tr("File not found:") << " " << path << "\n";
            return 1;
        }
        std::ifstream f(path);
        try {
            auto j = nlohmann::json::parse(f);
            static const std::vector<std::string> known = {
                "min_verdict", "min_confidence", "min_critical_coverage",
                "fail_on_regression", "max_drift", "fail_on_escalation",
                "fail_on_budget_exceeded"
            };
            int warnings = 0;
            for (const auto& [key, _] : j.items()) {
                if (std::find(known.begin(), known.end(), key) == known.end()) {
                    std::cerr << "  " << term::icon_warn() << " " << tr("Unknown field:") << " " << key << "\n";
                    ++warnings;
                }
            }
            std::cout << term::icon_ok() << " " << tr("Valid policy JSON");
            if (warnings > 0) {
                std::cout << " (" << warnings << " " << tr("unknown fields") << ")";
            }
            std::cout << "\n";
            return 0;
        } catch (const nlohmann::json::exception& e) {
            std::cerr << term::icon_fail() << " " << tr("Invalid JSON:") << " " << e.what() << "\n";
            return 1;
        }
    }

    // --- policy check ---
    if (sub == "check") {
        // Collect remaining args after "check"
        std::vector<std::string> check_args;
        std::string target;
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].starts_with("--") && target.empty()) {
                target = args[i];
            } else {
                check_args.push_back(args[i]);
            }
        }

        // With target: delegate to cmd_check with --policy
        if (!target.empty()) {
            std::vector<std::string> delegated = {target, "--policy"};
            for (const auto& a : check_args) delegated.push_back(a);
            return cmd_check(ctx, delegated);
        }

        // Without target: check latest artifact against policy
        auto artifact_path = fs::path(ctx.euxis_home) / "data" / "runtime" / "sessions" / "latest_verdict.json";
        if (!fs::exists(artifact_path)) {
            std::cerr << tr("No verdict artifact found. Run 'euxis check' first.") << "\n";
            return 1;
        }
        std::ifstream af(artifact_path);
        nlohmann::json artifact;
        try { artifact = nlohmann::json::parse(af); }
        catch (...) {
            std::cerr << tr("Failed to parse verdict artifact.") << "\n";
            return 1;
        }

        ConfigLoader loader(ctx.data_dir);
        auto policy_json = loader.load("config/policy.json");
        if (!policy_json) {
            std::cerr << tr("No policy file found.") << "\n";
            return 1;
        }

        // Evaluate policy gates against artifact
        int violations = 0;
        auto check_gate = [&](const std::string& gate, bool passed) {
            if (passed) {
                std::cout << "  " << term::icon_ok() << " " << gate << "\n";
            } else {
                std::cout << "  " << term::icon_fail() << " " << gate << "\n";
                ++violations;
            }
        };

        std::cout << term::bold(tr("Policy Check")) << " (" << tr("latest artifact") << ")\n";

        if (policy_json->contains("min_confidence")) {
            int min_conf = (*policy_json)["min_confidence"].get<int>();
            int actual = artifact.value("confidence", 0);
            check_gate("min_confidence >= " + std::to_string(min_conf),
                        actual >= min_conf);
        }
        if (policy_json->contains("min_verdict")) {
            std::string min_v = (*policy_json)["min_verdict"].get<std::string>();
            std::string actual = artifact.value("verdict", "UNKNOWN");
            // Verdict ordering: BLOCKED < INCONCLUSIVE < CAUTION < TRUSTED WITH GAPS < TRUSTED
            static const std::vector<std::string> ordering = {
                "BLOCKED", "INCONCLUSIVE", "CAUTION", "TRUSTED WITH GAPS", "TRUSTED"
            };
            auto min_it = std::find(ordering.begin(), ordering.end(), min_v);
            auto act_it = std::find(ordering.begin(), ordering.end(), actual);
            bool passed = (act_it != ordering.end() && min_it != ordering.end() &&
                           act_it >= min_it);
            check_gate("min_verdict >= " + min_v + " (actual: " + actual + ")", passed);
        }
        if (policy_json->contains("fail_on_budget_exceeded")) {
            bool gate = (*policy_json)["fail_on_budget_exceeded"].get<bool>();
            if (gate) {
                bool exceeded = artifact.value("budget_exceeded", false);
                check_gate("budget_not_exceeded", !exceeded);
            }
        }

        std::cout << "\n";
        if (violations > 0) {
            std::cout << term::red(std::to_string(violations) + " policy violation(s)") << "\n";
            return 1;
        }
        std::cout << term::green("All policy gates passed") << "\n";
        return 0;
    }

    std::cerr << tr("Unknown policy subcommand:") << " " << sub << "\n"
              << tr("Usage: euxis policy <check|show|validate>") << "\n";
    return 2;
}

} // namespace euxis::cli::cmd
