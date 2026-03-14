#include "euxis/cli/engine.hpp"
#include "euxis/cli/i18n.hpp"
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
#include <map>
#include <print>

namespace euxis::cli {

using euxis::cli::i18n::tr;

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
    ctx_.data_dir = (std::filesystem::path(ctx_.euxis_home) / "data").string();
    ctx_.no_color = !terminal::colors_enabled();

    register_commands();
}

void Engine::register_commands() {
    // System (7)
    commands_.push_back({"doctor",                 tr("System"),         tr("Run installation diagnostics"),        cmd::cmd_doctor});
    commands_.push_back({"fix",                    tr("System"),         tr("Autonomous environment self-repair"),  cmd::cmd_fix});
    commands_.push_back({"health",                 tr("System"),         tr("Fleet integrity check (10-point)"),    cmd::cmd_health});
    commands_.push_back({"verify",                 tr("System"),         tr("Verify agent prompt integrity"),       cmd::cmd_verify});
    commands_.push_back({"lint",                   tr("System"),         tr("Lint agent prompts and configs"),      cmd::cmd_lint});
    commands_.push_back({"shell-lint",             tr("System"),         tr("Lint shell scripts (shellcheck)"),     cmd::cmd_shell_lint});
    commands_.push_back({"verify-all",             tr("System"),         tr("Run all verification checks"),        cmd::cmd_verify_all});
    commands_.push_back({"cross-platform-verify",  tr("System"),         tr("Cross-platform compatibility check"), cmd::cmd_cross_platform_verify});

    // Fleet (9)
    commands_.push_back({"agent",            tr("Fleet"),          tr("Manage agents (list/register/unregister)"),  cmd::cmd_agent});
    commands_.push_back({"agent-bootstrap",  tr("Fleet"),          tr("Bootstrap a new agent from template"),       cmd::cmd_agent_bootstrap});
    commands_.push_back({"squad",            tr("Fleet"),          tr("Squad orchestration (list/deploy/info)"),    cmd::cmd_squad});
    commands_.push_back({"combo",            tr("Fleet"),          tr("Run agent combo (sequential pipeline)"),     cmd::cmd_combo});
    commands_.push_back({"playbook",         tr("Fleet"),          tr("Execute a playbook manifest"),               cmd::cmd_playbook});
    commands_.push_back({"ci",               tr("Fleet"),          tr("CI-ready repo verdict (deterministic JSON)"),cmd::cmd_ci});
    commands_.push_back({"dispatch",         tr("Fleet"),          tr("Dispatch agents from architect manifest"),   cmd::cmd_dispatch});
    commands_.push_back({"council",          tr("Fleet"),          tr("Run multi-agent council deliberation"),      cmd::cmd_council});
    commands_.push_back({"loop",             tr("Fleet"),          tr("Agent feedback loop (iterate to quality)"),  cmd::cmd_loop});
    commands_.push_back({"synthesize",       tr("Fleet"),          tr("Synthesize outputs from multiple agents"),   cmd::cmd_synthesize});

    // Knowledge (5)
    commands_.push_back({"cortex",     tr("Knowledge"),      tr("Semantic memory (remember/recall/forget)"),  cmd::cmd_cortex});
    commands_.push_back({"graph",      tr("Knowledge"),      tr("Knowledge graph operations"),                cmd::cmd_graph});
    commands_.push_back({"codex",      tr("Knowledge"),      tr("Template codex (list/render/validate)"),     cmd::cmd_codex});
    commands_.push_back({"omnigraph",  tr("Knowledge"),      tr("Workspace context graph for providers"),     cmd::cmd_omnigraph});
    commands_.push_back({"slash",      tr("Knowledge"),      tr("Slash command registry"),                    cmd::cmd_slash});

    // Infrastructure (5)
    commands_.push_back({"gateway",   tr("Infrastructure"),  tr("Start HTTP/WS gateway server"),              cmd::cmd_gateway});
    commands_.push_back({"bus",       tr("Infrastructure"),  tr("Message bus operations"),                     cmd::cmd_bus});
    commands_.push_back({"daemon",    tr("Infrastructure"),  tr("Background daemon management"),              cmd::cmd_daemon});
    commands_.push_back({"deploy",    tr("Infrastructure"),  tr("Deploy agent configurations"),               cmd::cmd_deploy});
    commands_.push_back({"optimize",  tr("Infrastructure"),  tr("Optimize runtime performance"),              cmd::cmd_optimize});

    // Development (8)
    commands_.push_back({"bench",              tr("Development"),    tr("Run performance benchmarks"),                cmd::cmd_bench});
    commands_.push_back({"hooks",              tr("Development"),    tr("Manage git hooks"),                          cmd::cmd_hooks});
    commands_.push_back({"setup-shell-hooks",  tr("Development"),    tr("Install shell integration hooks"),           cmd::cmd_setup_shell_hooks});
    commands_.push_back({"git-guard",          tr("Development"),    tr("Pre-commit/push safety checks"),             cmd::cmd_git_guard});
    commands_.push_back({"license-check",      tr("Development"),    tr("Check license compliance"),                  cmd::cmd_license_check});
    commands_.push_back({"docs-test",          tr("Development"),    tr("Test documentation examples"),               cmd::cmd_docs_test});
    commands_.push_back({"sync-docs",          tr("Development"),    tr("Sync docs to latest code state"),            cmd::cmd_sync_docs});
    commands_.push_back({"test-infra",         tr("Development"),    tr("Run infrastructure test suite"),             cmd::cmd_test_infra});

    // Specialized (12)
    commands_.push_back({"voice",            tr("Specialized"),    tr("Voice interface mode"),                       cmd::cmd_voice});
    commands_.push_back({"tui",              tr("Specialized"),    tr("Terminal UI (interactive dashboard)"),        cmd::cmd_tui});
    commands_.push_back({"gui",              tr("Specialized"),    tr("Launch the full Qt desktop GUI"),             cmd::cmd_gui});
    commands_.push_back({"polish",           tr("Specialized"),    tr("Polish agent outputs (formatting/style)"),   cmd::cmd_polish});
    commands_.push_back({"kaizen",           tr("Specialized"),    tr("Continuous improvement analysis"),            cmd::cmd_kaizen});
    commands_.push_back({"audit",            tr("Specialized"),    tr("Security and compliance audit"),              cmd::cmd_audit});
    commands_.push_back({"audit-run",        tr("Specialized"),    tr("Execute audit run with evidence"),            cmd::cmd_audit_run});
    commands_.push_back({"certify",          tr("Specialized"),    tr("Certify agent readiness"),                   cmd::cmd_certify});
    commands_.push_back({"evidence-verify",  tr("Specialized"),    tr("Verify audit evidence artifacts"),           cmd::cmd_evidence_verify});
    commands_.push_back({"gym",              tr("Specialized"),    tr("Agent training gym (eval + drill)"),          cmd::cmd_gym});
    commands_.push_back({"replay",           tr("Specialized"),    tr("Replay agent session from log"),             cmd::cmd_replay});
    commands_.push_back({"context-worker",   tr("Specialized"),    tr("Background context enrichment worker"),      cmd::cmd_context_worker});
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

    std::println(stderr, "{} {}", tr("Unknown command:"), cmd);
    std::println(stderr, "{}", tr("Run 'euxis help' for available commands."));
    return 1;
}

void Engine::print_version() {
    std::println("Euxis {} (C++23)", kVersion);
}

void Engine::print_help() const {
    terminal::print_banner();
    std::println("{}\n", tr("Usage: euxis [--no-color] [--json] [--verbose] <command> [options]"));

    // Group commands
    std::map<std::string, std::vector<const CommandEntry*>> groups;
    // Preserve insertion order with explicit ordering
    static const std::vector<std::string> group_order = {
        tr("System"), tr("Fleet"), tr("Knowledge"), tr("Infrastructure"), tr("Development"), tr("Specialized")
    };
    for (const auto& g : group_order) { groups[g] = {}; }
    for (const auto& entry : commands_) {
        groups[entry.group].push_back(&entry);
    }

    for (const auto& group_name : group_order) {
        const auto& cmds = groups[group_name];
        if (cmds.empty()) { continue; }

        std::println("{}:", terminal::bold(group_name));
        // Find max name length for alignment
        size_t max_len = 0;
        for (const auto* c : cmds) {
            max_len = std::max(max_len, c->name.size());
        }
        for (const auto* c : cmds) {
            std::println("  {}{}{}",
                         terminal::cyan(c->name),
                         std::string(max_len - c->name.size() + 2, ' '),
                         c->description);
        }
        std::println("");
    }

    std::println("{}", tr("Global flags:"));
    std::println("  --no-color  {}", tr("Disable colored output"));
    std::println("  --json      {}", tr("JSON output mode"));
    std::println("  --verbose   {}", tr("Verbose logging"));
    std::println("  --version   {}", tr("Show version"));
    std::println("  --help      {}", tr("Show this help"));
}

} // namespace euxis::cli
