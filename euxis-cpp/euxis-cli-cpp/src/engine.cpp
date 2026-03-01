#include "euxis/cli/engine.hpp"
#include "euxis/cli/terminal.hpp"
#include "euxis/cli/cmd/system.hpp"
#include "euxis/cli/cmd/fleet.hpp"
#include "euxis/cli/cmd/knowledge.hpp"
#include "euxis/cli/cmd/infra.hpp"
#include "euxis/cli/cmd/dev.hpp"
#include "euxis/cli/cmd/specialized.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>

namespace euxis::cli {
namespace {

constexpr auto kVersion = "v0.0.4";

} // namespace

Engine::Engine(const std::string& euxis_home) {
    if (!euxis_home.empty()) {
        ctx_.euxis_home = euxis_home;
    } else {
        const char* home = std::getenv("EUXIS_HOME");
        if (home) {
            ctx_.euxis_home = home;
        } else {
            const char* user_home = std::getenv("HOME");
            ctx_.euxis_home =
                (std::filesystem::path(user_home ? user_home : "/tmp") / ".euxis")
                    .string();
        }
    }
    ctx_.data_dir = (std::filesystem::path(ctx_.euxis_home) / "euxis-data").string();
    ctx_.no_color = !terminal::colors_enabled();

    register_commands();
}

void Engine::register_commands() {
    // System (7)
    commands_.push_back({"doctor",                 "System",         "Run installation diagnostics",        cmd::cmd_doctor});
    commands_.push_back({"health",                 "System",         "Fleet integrity check (10-point)",    cmd::cmd_health});
    commands_.push_back({"verify",                 "System",         "Verify agent prompt integrity",       cmd::cmd_verify});
    commands_.push_back({"lint",                   "System",         "Lint agent prompts and configs",      cmd::cmd_lint});
    commands_.push_back({"shell-lint",             "System",         "Lint shell scripts (shellcheck)",     cmd::cmd_shell_lint});
    commands_.push_back({"verify-all",             "System",         "Run all verification checks",        cmd::cmd_verify_all});
    commands_.push_back({"cross-platform-verify",  "System",         "Cross-platform compatibility check", cmd::cmd_cross_platform_verify});

    // Fleet (9)
    commands_.push_back({"agent",            "Fleet",          "Manage agents (list/register/unregister)",  cmd::cmd_agent});
    commands_.push_back({"agent-bootstrap",  "Fleet",          "Bootstrap a new agent from template",       cmd::cmd_agent_bootstrap});
    commands_.push_back({"squad",            "Fleet",          "Squad orchestration (list/deploy/info)",    cmd::cmd_squad});
    commands_.push_back({"combo",            "Fleet",          "Run agent combo (sequential pipeline)",     cmd::cmd_combo});
    commands_.push_back({"playbook",         "Fleet",          "Execute a playbook manifest",               cmd::cmd_playbook});
    commands_.push_back({"dispatch",         "Fleet",          "Dispatch agents from architect manifest",   cmd::cmd_dispatch});
    commands_.push_back({"council",          "Fleet",          "Run multi-agent council deliberation",      cmd::cmd_council});
    commands_.push_back({"loop",             "Fleet",          "Agent feedback loop (iterate to quality)",  cmd::cmd_loop});
    commands_.push_back({"synthesize",       "Fleet",          "Synthesize outputs from multiple agents",   cmd::cmd_synthesize});

    // Knowledge (5)
    commands_.push_back({"cortex",     "Knowledge",      "Semantic memory (remember/recall/forget)",  cmd::cmd_cortex});
    commands_.push_back({"graph",      "Knowledge",      "Knowledge graph operations",                cmd::cmd_graph});
    commands_.push_back({"codex",      "Knowledge",      "Template codex (list/render/validate)",     cmd::cmd_codex});
    commands_.push_back({"omnigraph",  "Knowledge",      "Workspace context graph for providers",     cmd::cmd_omnigraph});
    commands_.push_back({"slash",      "Knowledge",      "Slash command registry",                    cmd::cmd_slash});

    // Infrastructure (5)
    commands_.push_back({"gateway",   "Infrastructure",  "Start HTTP/WS gateway server",              cmd::cmd_gateway});
    commands_.push_back({"bus",       "Infrastructure",  "Message bus operations",                     cmd::cmd_bus});
    commands_.push_back({"daemon",    "Infrastructure",  "Background daemon management",              cmd::cmd_daemon});
    commands_.push_back({"deploy",    "Infrastructure",  "Deploy agent configurations",               cmd::cmd_deploy});
    commands_.push_back({"optimize",  "Infrastructure",  "Optimize runtime performance",              cmd::cmd_optimize});

    // Development (8)
    commands_.push_back({"bench",              "Development",    "Run performance benchmarks",                cmd::cmd_bench});
    commands_.push_back({"hooks",              "Development",    "Manage git hooks",                          cmd::cmd_hooks});
    commands_.push_back({"setup-shell-hooks",  "Development",    "Install shell integration hooks",           cmd::cmd_setup_shell_hooks});
    commands_.push_back({"git-guard",          "Development",    "Pre-commit/push safety checks",             cmd::cmd_git_guard});
    commands_.push_back({"license-check",      "Development",    "Check license compliance",                  cmd::cmd_license_check});
    commands_.push_back({"docs-test",          "Development",    "Test documentation examples",               cmd::cmd_docs_test});
    commands_.push_back({"sync-docs",          "Development",    "Sync docs to latest code state",            cmd::cmd_sync_docs});
    commands_.push_back({"test-infra",         "Development",    "Run infrastructure test suite",             cmd::cmd_test_infra});

    // Specialized (11)
    commands_.push_back({"voice",            "Specialized",    "Voice interface mode",                       cmd::cmd_voice});
    commands_.push_back({"tui",              "Specialized",    "Terminal UI (interactive dashboard)",        cmd::cmd_tui});
    commands_.push_back({"polish",           "Specialized",    "Polish agent outputs (formatting/style)",   cmd::cmd_polish});
    commands_.push_back({"kaizen",           "Specialized",    "Continuous improvement analysis",            cmd::cmd_kaizen});
    commands_.push_back({"audit",            "Specialized",    "Security and compliance audit",              cmd::cmd_audit});
    commands_.push_back({"audit-run",        "Specialized",    "Execute audit run with evidence",            cmd::cmd_audit_run});
    commands_.push_back({"certify",          "Specialized",    "Certify agent readiness",                   cmd::cmd_certify});
    commands_.push_back({"evidence-verify",  "Specialized",    "Verify audit evidence artifacts",           cmd::cmd_evidence_verify});
    commands_.push_back({"gym",              "Specialized",    "Agent training gym (eval + drill)",          cmd::cmd_gym});
    commands_.push_back({"replay",           "Specialized",    "Replay agent session from log",             cmd::cmd_replay});
    commands_.push_back({"context-worker",   "Specialized",    "Background context enrichment worker",      cmd::cmd_context_worker});
}

auto Engine::run(const std::vector<std::string>& args) -> int {
    if (args.empty()) {
        print_version();
        print_help();
        return 0;
    }

    // Parse global flags
    std::vector<std::string> remaining;
    for (const auto& arg : args) {
        if (arg == "--no-color") {
            ctx_.no_color = true;
        } else if (arg == "--json") {
            ctx_.json_output = true;
        } else if (arg == "--verbose" || arg == "-v") {
            ctx_.verbose = true;
        } else {
            remaining.push_back(arg);
        }
    }

    if (remaining.empty()) {
        print_version();
        print_help();
        return 0;
    }

    const auto& cmd = remaining[0];

    // Built-in meta commands
    if (cmd == "version" || cmd == "--version" || cmd == "-V") {
        print_version();
        return 0;
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_version();
        print_help();
        return 0;
    }

    // Table-driven dispatch
    std::vector<std::string> sub_args(remaining.begin() + 1, remaining.end());
    for (const auto& entry : commands_) {
        if (entry.name == cmd) {
            return entry.handler(ctx_, sub_args);
        }
    }

    std::cerr << "Unknown command: " << cmd << "\n"
              << "Run 'euxis help' for available commands.\n";
    return 1;
}

void Engine::print_version() {
    std::cout << "Euxis " << kVersion << " (C++23)\n";
}

void Engine::print_help() const {
    terminal::print_banner();
    std::cout << "Usage: euxis [--no-color] [--json] [--verbose] <command> [options]\n\n";

    // Group commands
    std::map<std::string, std::vector<const CommandEntry*>> groups;
    // Preserve insertion order with explicit ordering
    static const std::vector<std::string> group_order = {
        "System", "Fleet", "Knowledge", "Infrastructure", "Development", "Specialized"
    };
    for (const auto& g : group_order) groups[g] = {};
    for (const auto& entry : commands_) {
        groups[entry.group].push_back(&entry);
    }

    for (const auto& group_name : group_order) {
        const auto& cmds = groups[group_name];
        if (cmds.empty()) continue;

        std::cout << terminal::bold(group_name) << ":\n";
        // Find max name length for alignment
        size_t max_len = 0;
        for (const auto* c : cmds) {
            max_len = std::max(max_len, c->name.size());
        }
        for (const auto* c : cmds) {
            std::cout << "  " << terminal::cyan(c->name)
                      << std::string(max_len - c->name.size() + 2, ' ')
                      << c->description << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "Global flags:\n"
              << "  --no-color  Disable colored output\n"
              << "  --json      JSON output mode\n"
              << "  --verbose   Verbose logging\n"
              << "  --version   Show version\n"
              << "  --help      Show this help\n";
}

} // namespace euxis::cli
