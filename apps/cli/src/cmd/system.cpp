#include "euxis/cli/cmd/system.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/registry_client.hpp"
#include "euxis/cli/terminal.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

using euxis::cli::i18n::tr;

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;
namespace term = terminal;

struct CheckResult {
    std::string name;
    bool passed{false};
    std::string detail;
    bool optional{false};  // Optional checks show warning icon instead of fail
};

void print_check(const CheckResult& r) {
    if (r.passed) {
        std::cout << "  " << term::icon_ok();
    } else if (r.optional) {
        std::cout << "  " << term::icon_warn();
    } else {
        std::cout << "  " << term::icon_fail();
    }
    std::cout << " " << r.name;
    if (!r.detail.empty()) std::cout << " " << term::dim("(" + r.detail + ")");
    std::cout << "\n";
}

} // namespace

// --- doctor ---

int cmd_doctor(Context& ctx, const std::vector<std::string>& args) {
    bool fix_mode = false;
    for (const auto& a : args) {
        if (a == "--fix") fix_mode = true;
    }

    std::cout << term::bold(tr("Euxis Doctor")) << "\n\n";

    // System info header
    {
        auto hostname = Process::run("uname", {"-n"});
        auto kernel   = Process::run("uname", {"-r"});
        auto arch     = Process::run("uname", {"-m"});

        auto trim = [](std::string s) {
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                s.pop_back();
            return s;
        };

        std::cout << term::bold(tr("System:")) << "\n";
        std::cout << "  " << tr("Hostname:") << "     " << trim(hostname.stdout_output) << "\n";
        std::cout << "  " << tr("Kernel:") << "       " << trim(kernel.stdout_output) << "\n";
        std::cout << "  " << tr("Architecture:") << " " << trim(arch.stdout_output) << "\n\n";
    }

    std::vector<CheckResult> checks;
    std::vector<std::string> fixable;
    int failures = 0;

    // 1. Platform
    {
        CheckResult r{tr("Platform"), true, ""};
#ifdef __linux__
        r.detail = "Linux";
#elif defined(__APPLE__)
        r.detail = "macOS";
#else
        r.detail = "Unknown";
#endif
        checks.push_back(r);
    }

    // 2. Shell
    {
        const char* shell = std::getenv("SHELL");
        checks.push_back({tr("Shell"), shell != nullptr, shell ? shell : tr("not set")});
    }

    // 3. Required dependencies (and system locks first)
    {
        static const std::vector<std::pair<std::string, std::string>> locks = {
            {"/var/lib/pacman/db.lck", "sudo rm /var/lib/pacman/db.lck"},
            {"/var/lib/dpkg/lock-frontend", "sudo rm /var/lib/dpkg/lock-frontend"},
            {"/var/lib/apt/lists/lock", "sudo rm /var/lib/apt/lists/lock"}
        };

        for (const auto& [path, fix] : locks) {
            if (fs::exists(path)) {
                checks.push_back({tr("stale lock: ") + path, false, tr("lock exists")});
                ++failures;
                fixable.push_back(fix);
            }
        }
    }

    static const std::vector<std::pair<std::string, bool>> deps = {
        {"git", true}, {"sqlite3", true}, {"jq", false}, {"python3", false},
        {"pandoc", false}, {"docker", false}
    };
    for (const auto& [name, required] : deps) {
        bool found = Process::available(name);
        CheckResult r{name, found || !required, found ? tr("found") : tr("not found")};
        if (!found && required) {
            ++failures;
            fixable.push_back(name);
        }
        checks.push_back(r);
    }

    // 4. EUXIS_HOME
    {
        bool exists = fs::is_directory(ctx.euxis_home);
        checks.push_back({tr("EUXIS_HOME"), exists, ctx.euxis_home});
        if (!exists) ++failures;
    }

    // 5. Core directories
    static const std::vector<std::string> core_dirs = {
        "data", 
        "data/agents", 
        "data/config",
        "data/runtime",
        "data/runtime/memory",
        "data/runtime/memory/cortex",
        "data/runtime/memory/cortex/db",
        "data/runtime/bus",
        "data/runtime/bus/pipes",
        "data/runtime/context",
        "data/runtime/metrics"
    };

    for (const auto& dir : core_dirs) {
        auto path = fs::path(ctx.euxis_home) / dir;
        bool exists = fs::is_directory(path);
        checks.push_back({std::string(tr("dir: ")) + dir, exists, exists ? tr("ok") : tr("missing")});
        if (!exists) {
            ++failures;
            fixable.push_back(std::string("mkdir -p \"") + path.string() + "\"");
        }
    }

    // 6. Registry
    {
        auto json_path = fs::path(ctx.data_dir) / "agents" / "registry.json";
        auto db_path = fs::path(ctx.data_dir) / "agents" / "registry.db";
        bool has_json = fs::exists(json_path);
        bool has_db = fs::exists(db_path);
        checks.push_back({"registry.json", has_json, has_json ? tr("ok") : tr("missing")});
        checks.push_back({"registry.db", has_db, has_db ? tr("ok") : tr("optional, missing")});
        if (!has_json) {
            ++failures;
            // No simple auto-fix for missing registry, but we can suggest re-running install
            fixable.push_back(tr("Run install.sh to restore default registry"));
        }
    }

    // 7. Providers & Tools
    // Required: at least one AI provider must be available
    // Optional: individual providers are not required
    static const std::vector<std::pair<std::string, std::string>> required_tools = {
        {"shellcheck", "sudo pacman -S --noconfirm shellcheck # (or your pkg manager)"}
    };
    struct ProviderInfo { std::string name; std::string fix; };
    static const std::vector<ProviderInfo> providers = {
        {"claude", "npm install -g @anthropic-ai/claude-code"},
        {"gemini", "npm install -g @google/gemini-cli"},
        {"codex", "npm install -g @openai/codex"},
        {"ollama", "https://ollama.com/download"},
    };

    int providers_available = 0;
    for (const auto& p : providers) {
        bool found = Process::available(p.name);
        if (found) ++providers_available;
        checks.push_back({tr("provider: ") + p.name, found,
                          found ? tr("available") : tr("not installed (optional)"),
                          /*optional=*/!found});
    }
    if (providers_available == 0) {
        ++failures;
        fixable.push_back(tr("Install at least one AI provider (ollama, claude, gemini)"));
    }
    checks.push_back({tr("AI providers"), providers_available > 0,
                      std::to_string(providers_available) + "/4 " + tr("available")});

    for (const auto& [name, fix] : required_tools) {
        bool found = Process::available(name);
        checks.push_back({tr("tool: ") + name, found, found ? tr("available") : tr("not found")});
        if (!found) {
            ++failures;
            fixable.push_back(fix);
        }
    }

    // 8. Terminal (informational — not counted as failures)
    {
        const char* term_env = std::getenv("TERM");
        checks.push_back({tr("TERM"), term_env != nullptr, term_env ? term_env : tr("not set")});
        bool colors = term::colors_enabled();
        checks.push_back({tr("Colors"), true, colors ? tr("yes") : tr("no (non-TTY)")});
        checks.push_back({tr("CI mode"), true, term::is_ci() ? tr("yes") : tr("no")});
    }

    // 9. Disk space
    {
        std::error_code ec;
        auto space_info = fs::space(ctx.euxis_home, ec);
        if (!ec) {
            auto avail_mb = space_info.available / (1024ULL * 1024);
            bool ok = avail_mb >= 100;
            checks.push_back({tr("Disk space"), ok,
                               std::to_string(avail_mb) + " " + tr("MB available")});
            if (!ok) {
                ++failures;
                std::cout << term::yellow("  " + tr("Warning: less than 100 MB free on EUXIS_HOME")) << "\n";
            }
        } else {
            checks.push_back({tr("Disk space"), false, tr("unable to query")});
            ++failures;
        }
    }

    // Print results
    std::cout << term::bold(tr("Checks:")) << "\n";
    for (const auto& c : checks) print_check(c);

    std::cout << "\n" << term::bold(tr("Summary: "));
    if (failures == 0) {
        std::cout << term::green(tr("All checks passed")) << "\n";
    } else {
        std::cout << term::red(std::to_string(failures) + " " + tr("issue(s) found")) << "\n";
    }

    // Fix mode
    if (fix_mode && !fixable.empty()) {
        std::cout << "\n" << term::bold(tr("Auto-fix:")) << "\n";
        for (const auto& cmd : fixable) {
            if (cmd.starts_with("mkdir")) {
                std::cout << "  " << tr("Proposing:") << " " << term::cyan(cmd) << " [y/N] ";
                std::string response;
                std::getline(std::cin, response);
                if (response == "y" || response == "Y") {
                    std::cout << "  " << tr("Running...") << "\n";
                    Process::shell(cmd);
                }
            } else if (cmd.find("install") != std::string::npos || cmd.find("pacman") != std::string::npos || cmd.find("sudo") != std::string::npos) {
                std::cout << "  " << tr("Proposing to run:") << " " << term::cyan(cmd) << " [y/N] ";
                std::string response;
                std::getline(std::cin, response);
                if (response == "y" || response == "Y") {
                    std::cout << "  " << tr("Executing command...") << "\n";
                    Process::shell_interactive(cmd);
                }
            } else {
                std::cout << "  " << tr("Manual fix required:") << " " << term::yellow(cmd) << "\n";
            }
        }
    } else if (!fixable.empty()) {
        std::cout << "  " << tr("Run with --fix to attempt auto-repair.") << "\n";
    }

    if (ctx.json_output) {
        nlohmann::json j;
        j["status"] = failures == 0 ? "healthy" : "degraded";
        j["failures"] = failures;
        j["checks"] = nlohmann::json::array();
        for (const auto& c : checks) {
            j["checks"].push_back({{"name", c.name}, {"passed", c.passed}, {"detail", c.detail}});
        }
        std::cout << j.dump(2) << "\n";
    }

    return failures > 0 ? 1 : 0;
}

int cmd_fix(Context& ctx, const std::vector<std::string>& /*args*/) {
    return cmd_doctor(ctx, {"--fix"});
}

// --- health ---

int cmd_health(Context& ctx, const std::vector<std::string>& args) {
    bool silent = false;
    for (const auto& a : args) {
        if (a == "--silent") silent = true;
    }

    if (!silent) std::cout << term::bold(tr("Fleet Health Check")) << "\n\n";

    int failures = 0;
    int warnings = 0;

    RegistryClient registry(ctx.data_dir);

    // 1. Registry available
    {
        bool ok = registry.has_json() || registry.has_sqlite();
        if (!silent) {
            std::cout << "  " << (ok ? term::icon_ok() : term::icon_fail())
                      << " " << tr("Registry available")
                      << " (SQLite: " << (registry.has_sqlite() ? tr("yes") : tr("no"))
                      << ", JSON: " << (registry.has_json() ? tr("yes") : tr("no")) << ")\n";
        }
        if (!ok) ++failures;
    }

    // 2. Agent count
    {
        int count = registry.agent_count();
        if (!silent) {
            std::cout << "  " << (count > 0 ? term::icon_ok() : term::icon_warn())
                      << " " << tr("Agents registered:") << " " << count << "\n";
        }
        if (count == 0) ++warnings;
    }

    // 3. Prompt files exist
    {
        auto agents = registry.list_agents();
        int missing = 0;
        for (const auto& a : agents) {
            if (!a.prompt_path.empty()) {
                auto full_path = fs::path(ctx.data_dir) / a.prompt_path;
                if (!fs::exists(full_path)) ++missing;
            }
        }
        bool ok = missing == 0;
        if (!silent) {
            std::cout << "  " << (ok ? term::icon_ok() : term::icon_warn())
                      << " " << tr("Prompt files") << (ok ? " " + tr("all present") : " " + std::to_string(missing) + " " + tr("missing")) << "\n";
        }
        if (!ok) ++warnings;
    }

    // 4. Provider connectivity
    {
        int found = 0;
        static const std::vector<std::string> providers = {"claude", "gemini", "codex", "ollama", "goose"};
        for (const auto& p : providers) {
            if (Process::available(p)) ++found;
        }
        if (!silent) {
            std::cout << "  " << (found > 0 ? term::icon_ok() : term::icon_fail())
                      << " " << tr("Providers available:") << " " << found << "/" << providers.size() << "\n";
        }
        if (found == 0) ++failures;
    }

    // 5. Core directories
    {
        bool ok = fs::is_directory(fs::path(ctx.euxis_home) / "data");
        if (!silent) {
            std::cout << "  " << (ok ? term::icon_ok() : term::icon_fail())
                      << " " << tr("Data directory") << "\n";
        }
        if (!ok) ++failures;
    }

    // 6. Config integrity
    {
        ConfigLoader loader(ctx.data_dir);
        bool ok = loader.exists("config/router.json") || loader.exists("agents/registry.json");
        if (!silent) {
            std::cout << "  " << (ok ? term::icon_ok() : term::icon_warn())
                      << " " << tr("Config files") << "\n";
        }
        if (!ok) ++warnings;
    }

    // 7. Squads defined
    {
        auto squads = registry.list_squads();
        if (!silent) {
            std::cout << "  " << (!squads.empty() ? term::icon_ok() : term::icon_warn())
                      << " " << tr("Squads defined:") << " " << squads.size() << "\n";
        }
    }

    // 8. Bus pipes
    {
        auto bus_dir = fs::path(ctx.euxis_home) / "data/runtime" / "data" / "bus" / "pipes";
        int pipe_count = 0;
        if (fs::is_directory(bus_dir)) {
            for (const auto& entry : fs::directory_iterator(bus_dir)) {
                if (entry.is_regular_file()) ++pipe_count;
            }
        }
        if (!silent) {
            std::cout << "  " << term::icon_info()
                      << " " << tr("Bus pipes:") << " " << pipe_count << "\n";
        }
    }

    // 9. Cortex check
    {
        auto cortex_db = fs::path(ctx.euxis_home) / "data/runtime" / "memory" / "cortex" / "db";
        bool ok = fs::is_directory(cortex_db);
        if (!silent) {
            std::cout << "  " << (ok ? term::icon_ok() : term::icon_warn())
                      << " " << tr("Cortex DB") << (ok ? " " + tr("present") : " " + tr("not found")) << "\n";
        }
    }

    // 10. Certification tools
    {
        bool has_lint = Process::available("shellcheck");
        if (!silent) {
            std::cout << "  " << (has_lint ? term::icon_ok() : term::icon_warn())
                      << " shellcheck" << (has_lint ? " " + tr("available") : " " + tr("not found")) << "\n";
        }
    }

    // 11. Orphan agent detection
    {
        auto agents = registry.list_agents();
        int orphans = 0;
        for (const auto& a : agents) {
            if (a.prompt_path.empty()) {
                ++orphans;
            } else {
                auto full_path = fs::path(ctx.data_dir) / a.prompt_path;
                if (!fs::exists(full_path)) ++orphans;
            }
        }
        bool ok = orphans == 0;
        if (!silent) {
            std::cout << "  " << (ok ? term::icon_ok() : term::icon_warn())
                      << " " << tr("Orphan agents:") << " " << orphans;
            if (!ok) std::cout << " " << tr("(prompt file missing)");
            std::cout << "\n";
        }
        if (!ok) ++warnings;
    }

    // 12. Agent naming convention
    {
        auto agents = registry.list_agents();
        const std::regex id_pattern("^[a-z][a-z0-9-]*$");
        int bad_names = 0;
        for (const auto& a : agents) {
            if (!std::regex_match(a.id, id_pattern)) {
                ++bad_names;
                if (!silent && ctx.verbose) {
                    std::cout << "    " << term::icon_warn()
                              << " " << tr("bad ID:") << " " << term::dim(a.id) << "\n";
                }
            }
        }
        bool ok = bad_names == 0;
        if (!silent) {
            std::cout << "  " << (ok ? term::icon_ok() : term::icon_warn())
                      << " " << tr("Agent naming convention")
                      << (ok ? "" : " (" + std::to_string(bad_names) + " " + tr("non-conforming") + ")")
                      << "\n";
        }
        if (!ok) ++warnings;
    }

    // Summary
    if (!silent) {
        std::cout << "\n";
        if (failures == 0 && warnings == 0) {
            std::cout << term::green(tr("Fleet healthy")) << "\n";
        } else if (failures == 0) {
            std::cout << term::yellow(std::to_string(warnings) + " " + tr("warning(s)")) << "\n";
        } else {
            std::cout << term::red(std::to_string(failures) + " " + tr("failure(s),") + " " +
                                   std::to_string(warnings) + " " + tr("warning(s)")) << "\n";
        }
    }

    if (ctx.json_output) {
        nlohmann::json j;
        j["status"] = failures == 0 ? "healthy" : "degraded";
        j["failures"] = failures;
        j["warnings"] = warnings;
        j["agents"] = registry.agent_count();
        j["squads"] = registry.list_squads().size();
        std::cout << j.dump(2) << "\n";
    }

    return failures > 0 ? 1 : 0;
}

// --- verify ---

int cmd_verify(Context& ctx, const std::vector<std::string>& /*args*/) {
    std::cout << term::bold(tr("Verify Agent Prompts")) << "\n\n";

    RegistryClient registry(ctx.data_dir);
    auto agents = registry.list_agents();
    int issues = 0;

    for (const auto& agent : agents) {
        bool ok = true;
        std::string detail;

        // Check prompt file exists
        if (!agent.prompt_path.empty()) {
            auto full = fs::path(ctx.data_dir) / agent.prompt_path;
            if (!fs::exists(full)) {
                ok = false;
                detail = tr("prompt file missing");
            } else {
                // Check for required frontmatter fields
                std::ifstream f(full);
                std::string content((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
                for (const auto& field : {"agent_id", "role", "version"}) {
                    if (content.find(field) == std::string::npos) {
                        ok = false;
                        detail += std::string(detail.empty() ? "" : ", ") +
                                  tr("missing") + " " + field;
                    }
                }
            }
        } else {
            ok = false;
            detail = tr("no prompt path");
        }

        std::cout << "  " << (ok ? term::icon_ok() : term::icon_fail())
                  << " " << agent.id;
        if (!detail.empty()) std::cout << " " << term::dim("(" + detail + ")");
        std::cout << "\n";
        if (!ok) ++issues;
    }

    std::cout << "\n" << (issues == 0 ? term::green(tr("All verified")) :
                          term::red(std::to_string(issues) + " " + tr("issue(s)"))) << "\n";
    return issues > 0 ? 1 : 0;
}

// --- lint ---

int cmd_lint(Context& ctx, const std::vector<std::string>& /*args*/) {
    std::cout << term::bold(tr("Lint Agent Configs")) << "\n\n";

    ConfigLoader loader(ctx.data_dir);
    int issues = 0;

    // Lint registry.json
    auto reg = loader.load("agents/registry.json");
    if (reg) {
        if (!reg->contains("agents") || !(*reg)["agents"].is_array()) {
            std::cout << "  " << term::icon_fail() << " registry.json: " << tr("missing 'agents' array") << "\n";
            ++issues;
        } else {
            std::cout << "  " << term::icon_ok() << " registry.json: " << tr("valid") << " ("
                      << (*reg)["agents"].size() << " " << tr("agents") << ")\n";
        }
    } else {
        std::cout << "  " << term::icon_warn() << " registry.json: " << tr("not found") << "\n";
    }

    // Lint router.json
    auto router = loader.load("config/router.json");
    if (router) {
        std::cout << "  " << term::icon_ok() << " router.json: " << tr("present") << "\n";
    } else {
        std::cout << "  " << term::icon_warn() << " router.json: " << tr("not found (using defaults)") << "\n";
    }

    // Lint codex.json
    auto codex = loader.load("config/codex.json");
    if (codex) {
        std::cout << "  " << term::icon_ok() << " codex.json: " << tr("present") << "\n";
    }

    std::cout << "\n" << (issues == 0 ? term::green(tr("Lint passed")) :
                          term::red(std::to_string(issues) + " " + tr("issue(s)"))) << "\n";
    return issues > 0 ? 1 : 0;
}

// --- shell-lint ---

int cmd_shell_lint(Context& ctx, const std::vector<std::string>& /*args*/) {
    std::cout << term::bold(tr("Shell Lint")) << "\n\n";

    if (!Process::available("shellcheck")) {
        std::cerr << term::icon_fail() << " " << tr("shellcheck not found. Install it first.") << "\n";
        return 1;
    }

    auto bin_dir = fs::path(ctx.euxis_home) / "euxis-bin";
    if (!fs::is_directory(bin_dir)) {
        std::cout << term::icon_info() << " " << tr("No euxis-bin/ directory (already migrated to C++)") << "\n";
        return 0;
    }

    int issues = 0;
    for (const auto& entry : fs::directory_iterator(bin_dir)) {
        if (!entry.is_regular_file()) continue;
        auto path = entry.path();
        if (path.extension() == ".sh" || path.filename().string().starts_with("euxis-")) {
            auto result = Process::run("shellcheck", {"-S", "warning", path.string()});
            if (result.exit_code == 0) {
                std::cout << "  " << term::icon_ok() << " " << path.filename().string() << "\n";
            } else {
                std::cout << "  " << term::icon_fail() << " " << path.filename().string() << "\n";
                if (ctx.verbose && !result.stdout_output.empty()) {
                    std::cout << result.stdout_output;
                }
                ++issues;
            }
        }
    }

    std::cout << "\n" << (issues == 0 ? term::green(tr("All scripts pass")) :
                          term::red(std::to_string(issues) + " " + tr("script(s) with issues"))) << "\n";
    return issues > 0 ? 1 : 0;
}

// --- verify-all ---

int cmd_verify_all(Context& ctx, const std::vector<std::string>& /*args*/) {
    std::cout << term::bold(tr("Running All Verifications")) << "\n\n";

    int total_failures = 0;

    std::cout << "--- " << tr("Doctor") << " ---\n";
    total_failures += (cmd_doctor(ctx, {}) != 0 ? 1 : 0);

    std::cout << "\n--- " << tr("Health") << " ---\n";
    total_failures += (cmd_health(ctx, {}) != 0 ? 1 : 0);

    std::cout << "\n--- " << tr("Verify") << " ---\n";
    total_failures += (cmd_verify(ctx, {}) != 0 ? 1 : 0);

    std::cout << "\n--- " << tr("Lint") << " ---\n";
    total_failures += (cmd_lint(ctx, {}) != 0 ? 1 : 0);

    std::cout << "\n" << term::bold(tr("Total:")) << " ";
    if (total_failures == 0) {
        std::cout << term::green(tr("All verifications passed")) << "\n";
    } else {
        std::cout << term::red(std::to_string(total_failures) + " " + tr("verification group(s) failed")) << "\n";
    }

    return total_failures > 0 ? 1 : 0;
}

// --- cross-platform-verify ---

int cmd_cross_platform_verify(Context& ctx, const std::vector<std::string>& /*args*/) {
    std::cout << term::bold(tr("Cross-Platform Verification")) << "\n\n";

    int issues = 0;

    // Check filesystem case sensitivity
    {
        auto test_dir = fs::path(ctx.euxis_home) / ".cross-platform-test";
        fs::create_directories(test_dir);
        auto upper = test_dir / "TEST";
        auto lower = test_dir / "test";
        std::ofstream(upper) << "upper";
        bool case_sensitive = !fs::exists(lower) || fs::file_size(upper) != fs::file_size(lower);
        std::cout << "  " << term::icon_info() << " " << tr("Filesystem:") << " "
                  << (case_sensitive ? tr("case-sensitive") : tr("case-insensitive")) << "\n";
        fs::remove_all(test_dir);
    }

    // Check path separators
    std::cout << "  " << term::icon_info() << " " << tr("Path separator:") << " "
              << fs::path::preferred_separator << "\n";

    // Check available shells
    for (const auto& sh : {"bash", "zsh", "fish"}) {
        bool found = Process::available(sh);
        std::cout << "  " << (found ? term::icon_ok() : term::icon_skip())
                  << " " << sh << "\n";
    }

    // Check line endings
    {
        auto sample = fs::path(ctx.euxis_home) / "data" / "agents" / "registry.json";
        if (fs::exists(sample)) {
            std::ifstream f(sample, std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
            bool has_crlf = content.find("\r\n") != std::string::npos;
            std::cout << "  " << (!has_crlf ? term::icon_ok() : term::icon_warn())
                      << " " << tr("Line endings:") << " " << (has_crlf ? tr("CRLF (Windows)") : tr("LF (Unix)")) << "\n";
            if (has_crlf) ++issues;
        }
    }

    std::cout << "\n" << (issues == 0 ? term::green(tr("Cross-platform checks passed")) :
                          term::yellow(std::to_string(issues) + " " + tr("concern(s)"))) << "\n";
    return 0;
}

} // namespace euxis::cli::cmd
