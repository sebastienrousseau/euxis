#include "euxis/cli/cmd/system_card.hpp"
#include "euxis/cli/terminal.hpp"
#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/config_loader.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace euxis::cli::cmd {
namespace {
namespace fs = std::filesystem;
namespace term = terminal;
}

int cmd_system_card(Context& ctx, const std::vector<std::string>& args) {
    bool json_output = ctx.json_output;
    std::string output_path;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--json") json_output = true;
        else if (args[i] == "--output" && i + 1 < args.size()) output_path = args[++i];
        else if (args[i] == "--help") {
            std::cout << "Usage: euxis system-card [--json] [--output PATH]\n";
            std::cout << "  Generate a system card describing the euxis CLI capabilities.\n";
            return 0;
        }
    }

    ProviderRouter router(ctx.data_dir);

    nlohmann::json card;

    // Identity
    card["identity"] = {
        {"name", "euxis"},
        {"version", "0.0.2"},
        {"build_date", __DATE__},
        {"compiler",
#if defined(__clang__)
            "clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__)
#elif defined(__GNUC__)
            "gcc " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__)
#else
            "unknown"
#endif
        },
        {"cpp_standard", std::to_string(__cplusplus)}
    };

    // Capabilities
    card["capabilities"] = {
        {"modes", {"flash", "standard", "forensic"}},
        {"agent_count", 11},
        {"command_groups", {"Core", "Lifecycle", "System", "Fleet", "Knowledge", "Infrastructure", "Development", "Specialized"}}
    };

    // Providers
    auto providers = nlohmann::json::array();
    for (const auto& [name, tier] : std::vector<std::pair<std::string, std::string>>{
        {"ollama", "routine"}, {"gemini", "routine/data"}, {"claude", "code/reason"}, {"openai", "data"}}) {
        providers.push_back({{"name", name}, {"tiers", tier}});
    }
    card["providers"] = providers;

    // Accuracy — load targets if available
    auto targets_path = fs::path(ctx.data_dir) / "config" / "targets.json";
    if (fs::exists(targets_path)) {
        try {
            std::ifstream tf(targets_path);
            card["accuracy"] = {{"baseline_targets", nlohmann::json::parse(tf)}};
        } catch (...) {
            card["accuracy"] = {{"baseline_targets", "unavailable"}};
        }
    } else {
        card["accuracy"] = {{"baseline_targets", "not configured"}};
    }

    // Limitations
    card["limitations"] = {
        "SLA budgets constrain analysis depth (flash: 45s, standard: 180s, forensic: 600s)",
        "Provider availability depends on API keys and network connectivity",
        "Platform support: Linux (primary), macOS, WSL",
        "Verdict confidence is statistical, not deterministic"
    };

    // Security
    card["security"] = {
        {"audit_logging", true},
        {"hash_chained_logs", true},
        {"provenance_tracking", true},
        {"capability_tokens", true},
        {"seccomp_sandboxing",
#ifdef __linux__
            true
#else
            false
#endif
        }
    };

    // Compliance
    card["compliance"] = {
        {"frameworks", {"General", "SOC2", "ISO27001"}},
        {"eu_ai_act", {
            {"risk_level", "limited"},
            {"transparency_obligations", true},
            {"human_oversight", true}
        }}
    };

    if (json_output) {
        std::string json_str = card.dump(2);
        if (!output_path.empty()) {
            std::ofstream out(output_path);
            out << json_str;
            std::cout << "System card written to " << output_path << "\n";
        } else {
            std::cout << json_str << "\n";
        }
    } else {
        auto& out = std::cout;
        out << term::bold(term::cyan("=== EUXIS SYSTEM CARD ===")) << "\n\n";

        out << term::bold("Identity") << "\n";
        out << "  Name:      " << card["identity"]["name"].get<std::string>() << "\n";
        out << "  Version:   " << card["identity"]["version"].get<std::string>() << "\n";
        out << "  Compiler:  " << card["identity"]["compiler"].get<std::string>() << "\n";
        out << "  Built:     " << card["identity"]["build_date"].get<std::string>() << "\n\n";

        out << term::bold("Capabilities") << "\n";
        out << "  Modes:     flash, standard, forensic\n";
        out << "  Agents:    " << card["capabilities"]["agent_count"].get<int>() << "\n\n";

        out << term::bold("Providers") << "\n";
        for (const auto& p : card["providers"]) {
            out << "  " << p["name"].get<std::string>() << " (" << p["tiers"].get<std::string>() << ")\n";
        }
        out << "\n";

        out << term::bold("Limitations") << "\n";
        for (const auto& l : card["limitations"]) {
            out << "  - " << l.get<std::string>() << "\n";
        }
        out << "\n";

        out << term::bold("Security") << "\n";
        out << "  Audit logging:      " << (card["security"]["audit_logging"].get<bool>() ? "enabled" : "disabled") << "\n";
        out << "  Hash-chained logs:  " << (card["security"]["hash_chained_logs"].get<bool>() ? "enabled" : "disabled") << "\n";
        out << "  Seccomp sandbox:    " << (card["security"]["seccomp_sandboxing"].get<bool>() ? "enabled" : "disabled") << "\n\n";

        out << term::bold("Compliance") << "\n";
        out << "  Frameworks: General, SOC2, ISO27001\n";
        out << "  EU AI Act:  limited risk, transparency obligations met\n\n";

        if (!output_path.empty()) {
            std::ofstream file(output_path);
            file << card.dump(2);
            out << "System card also written to " << output_path << "\n";
        }
    }

    return 0;
}

} // namespace euxis::cli::cmd
