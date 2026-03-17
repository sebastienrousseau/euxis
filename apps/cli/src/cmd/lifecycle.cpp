#include "euxis/cli/cmd/lifecycle.hpp"
#include "euxis/cli/cmd/system.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/terminal.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

using euxis::cli::i18n::tr;

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;
namespace term = terminal;

constexpr auto kMarkerBegin = "# >>> euxis install >>>";
constexpr auto kMarkerEnd   = "# <<< euxis install <<<";

// --- helpers ---

auto detect_shell() -> std::string {
    const char* shell = std::getenv("SHELL");
    if (!shell) return "unknown";
    std::string s(shell);
    auto pos = s.rfind('/');
    return pos != std::string::npos ? s.substr(pos + 1) : s;
}

auto shell_config_path(const std::string& shell) -> fs::path {
    const char* home = std::getenv("HOME");
    if (!home) return {};
    fs::path h(home);
    if (shell == "bash") return h / ".bashrc";
    if (shell == "zsh")  return h / ".zshrc";
    if (shell == "fish") return h / ".config" / "fish" / "config.fish";
    return {};
}

auto has_config_entry(const fs::path& config_path, const std::string& marker) -> bool {
    if (!fs::exists(config_path)) return false;
    std::ifstream f(config_path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.find(marker) != std::string::npos) return true;
    }
    return false;
}

auto resolve_binary_path() -> fs::path {
#ifdef __APPLE__
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        std::error_code ec;
        auto resolved = fs::canonical(buf, ec);
        if (!ec) return resolved;
        return buf;
    }
#endif
    std::error_code ec;
    auto proc_path = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return proc_path;
    return {};
}

auto completions_source_dir(const std::string& euxis_home) -> fs::path {
    return fs::path(euxis_home) / "data" / "config" / "completions";
}

/// Detect install mode: source-checkout, local-installed, or unknown.
auto detect_install_mode(const std::string& euxis_home) -> std::string {
    if (fs::exists(fs::path(euxis_home) / ".git") &&
        fs::exists(fs::path(euxis_home) / "CMakeLists.txt")) {
        return "source-checkout";
    }
    if (fs::exists(fs::path(euxis_home) / "bin" / "euxis") &&
        !fs::exists(fs::path(euxis_home) / ".git")) {
        return "local-installed";
    }
    return "unknown";
}

/// Write shell config block with marker delimiters.
void write_shell_block(const fs::path& config_path, const std::string& shell) {
    std::ofstream f(config_path, std::ios::app);
    f << "\n" << kMarkerBegin << "\n";
    if (shell == "fish") {
        f << "set -gx EUXIS_HOME \"$HOME/.euxis\"\n";
        f << "fish_add_path -g \"$HOME/.euxis/bin\"\n";
    } else {
        f << "export EUXIS_HOME=\"$HOME/.euxis\"\n";
        f << "export PATH=\"$EUXIS_HOME/bin:$PATH\"\n";
    }
    f << kMarkerEnd << "\n";
}

/// Remove marker-delimited shell config block.
void remove_shell_block(const fs::path& config_path) {
    std::ifstream in(config_path);
    std::vector<std::string> lines;
    std::string line;
    bool in_block = false;
    while (std::getline(in, line)) {
        if (line.find(kMarkerBegin) != std::string::npos) {
            in_block = true;
            continue;
        }
        if (line.find(kMarkerEnd) != std::string::npos) {
            in_block = false;
            continue;
        }
        if (!in_block) lines.push_back(line);
    }
    in.close();

    // Remove trailing empty lines
    while (!lines.empty() && lines.back().empty()) lines.pop_back();

    std::ofstream out(config_path, std::ios::trunc);
    for (const auto& l : lines) out << l << "\n";
}

/// Install completions for a specific shell. Returns true if action was taken.
bool install_completions(const std::string& euxis_home, const std::string& shell,
                         bool dry_run, bool force) {
    auto comp_src = completions_source_dir(euxis_home);

    if (shell == "fish") {
        auto fish_comp = comp_src / "euxis.fish";
        const char* home = std::getenv("HOME");
        auto fish_dest = fs::path(home ? home : "/tmp") / ".config" / "fish" / "completions" / "euxis.fish";

        if (!fs::exists(fish_comp)) return false;
        bool dest_exists = fs::is_symlink(fish_dest) || fs::exists(fish_dest);
        if (dest_exists && !force) {
            std::cout << "  " << term::icon_ok() << " " << tr("Fish completions") << " " << term::dim(tr("(installed)")) << "\n";
            return false;
        }
        if (dry_run) {
            std::cout << "  " << term::icon_info() << " " << tr("Would symlink") << " " << fish_dest.string() << "\n";
        } else {
            fs::create_directories(fish_dest.parent_path());
            std::error_code ec;
            fs::remove(fish_dest, ec);
            fs::create_symlink(fish_comp, fish_dest, ec);
            if (ec) {
                std::cout << "  " << term::icon_fail() << " " << tr("Failed to install fish completions:") << " " << ec.message() << "\n";
            } else {
                std::cout << "  " << term::icon_ok() << " " << tr("Installed fish completions") << "\n";
            }
        }
        return true;
    }

    if (shell == "bash") {
        auto bash_comp = comp_src / "euxis.bash";
        if (!fs::exists(bash_comp)) return false;
        auto config = shell_config_path("bash");
        bool has_source = has_config_entry(config, "euxis.bash");
        if (has_source && !force) {
            std::cout << "  " << term::icon_ok() << " " << tr("Bash completions") << " " << term::dim(tr("(installed)")) << "\n";
            return false;
        }
        if (dry_run) {
            std::cout << "  " << term::icon_info() << " " << tr("Would add completion source to") << " " << config.string() << "\n";
        } else {
            std::ofstream f(config, std::ios::app);
            f << "\n# Euxis completions\nsource \"" << bash_comp.string() << "\"\n";
            std::cout << "  " << term::icon_ok() << " " << tr("Added bash completions to") << " " << config.string() << "\n";
        }
        return true;
    }

    if (shell == "zsh") {
        auto zsh_comp = comp_src / "euxis.zsh";
        if (!fs::exists(zsh_comp)) return false;
        const char* home = std::getenv("HOME");
        auto zsh_dest = fs::path(home ? home : "/tmp") / ".zsh" / "completions" / "_euxis";
        bool dest_exists = fs::is_symlink(zsh_dest) || fs::exists(zsh_dest);
        if (dest_exists && !force) {
            std::cout << "  " << term::icon_ok() << " " << tr("Zsh completions") << " " << term::dim(tr("(installed)")) << "\n";
            return false;
        }
        if (dry_run) {
            std::cout << "  " << term::icon_info() << " " << tr("Would symlink") << " " << zsh_dest.string() << "\n";
        } else {
            fs::create_directories(zsh_dest.parent_path());
            std::error_code ec;
            fs::remove(zsh_dest, ec);
            fs::create_symlink(zsh_comp, zsh_dest, ec);
            if (ec) {
                std::cout << "  " << term::icon_fail() << " " << tr("Failed to install zsh completions:") << " " << ec.message() << "\n";
            } else {
                std::cout << "  " << term::icon_ok() << " " << tr("Installed zsh completions") << "\n";
                if (!has_config_entry(shell_config_path("zsh"), ".zsh/completions")) {
                    auto zshrc = shell_config_path("zsh");
                    std::ofstream f(zshrc, std::ios::app);
                    f << "\n# Euxis zsh completions\n"
                      << "fpath=(~/.zsh/completions $fpath)\n"
                      << "autoload -Uz compinit && compinit\n";
                }
            }
        }
        return true;
    }

    return false;
}

} // namespace

// --- install ---

int cmd_install(Context& ctx, const std::vector<std::string>& args) {
    bool dry_run = false;
    bool force = false;
    bool shell_setup = false;
    bool no_completions = false;
    std::string shell_override;

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a == "--help" || a == "-h") {
            std::cerr << tr("Bootstrap local Euxis installation.") << "\n\n"
                      << tr("Usage:") << "\n"
                      << "  euxis install [options]\n\n"
                      << tr("Options:") << "\n"
                      << "  --dry-run          " << tr("Show what would be done without modifying anything") << "\n"
                      << "  --force            " << tr("Overwrite existing symlinks and config entries") << "\n"
                      << "  --shell-setup      " << tr("Also configure shell (PATH, EUXIS_HOME, completions)") << "\n"
                      << "  --shell <name>     " << tr("Shell to configure (fish, bash, zsh)") << "\n"
                      << "  --no-completions   " << tr("Skip completion installation") << "\n";
            return 0;
        }
        if (a == "--dry-run")        dry_run = true;
        if (a == "--force")          force = true;
        if (a == "--shell-setup")    shell_setup = true;
        if (a == "--no-completions") no_completions = true;
        if (a == "--shell" && i + 1 < args.size()) {
            shell_override = args[++i];
            shell_setup = true;  // --shell implies --shell-setup
        }
    }

    std::cout << term::bold(tr("Euxis Install")) << "\n\n";

    int actions = 0;

    // 1. Create EUXIS_HOME directory structure
    static const std::vector<std::string> dirs = {
        "bin",
        "data/agents",
        "data/config",
        "data/runtime/memory",
        "data/runtime/sessions",
        "data/runtime/verdicts",
    };

    for (const auto& dir : dirs) {
        auto path = fs::path(ctx.euxis_home) / dir;
        if (fs::is_directory(path)) {
            std::cout << "  " << term::icon_ok() << " " << dir << " " << term::dim(tr("(exists)")) << "\n";
        } else {
            if (dry_run) {
                std::cout << "  " << term::icon_info() << " " << tr("Would create") << " " << dir << "\n";
            } else {
                fs::create_directories(path);
                std::cout << "  " << term::icon_ok() << " " << tr("Created") << " " << dir << "\n";
            }
            ++actions;
        }
    }

    // 2. Symlink binary
    auto bin_link = fs::path(ctx.euxis_home) / "bin" / "euxis";
    auto binary = resolve_binary_path();

    if (!binary.empty()) {
        bool link_exists = fs::is_symlink(bin_link) || fs::exists(bin_link);
        if (link_exists && !force) {
            std::cout << "  " << term::icon_ok() << " " << tr("Binary symlink") << " " << term::dim(tr("(exists)")) << "\n";
        } else {
            if (dry_run) {
                std::cout << "  " << term::icon_info() << " " << tr("Would symlink") << " " << bin_link.string()
                          << " -> " << binary.string() << "\n";
            } else {
                std::error_code ec;
                fs::remove(bin_link, ec);
                fs::create_symlink(binary, bin_link, ec);
                if (ec) {
                    std::cout << "  " << term::icon_fail() << " " << tr("Failed to create symlink:") << " " << ec.message() << "\n";
                } else {
                    std::cout << "  " << term::icon_ok() << " " << tr("Symlinked") << " " << bin_link.string()
                              << " -> " << binary.string() << "\n";
                }
            }
            ++actions;
        }
    } else {
        std::cout << "  " << term::icon_warn() << " " << tr("Could not resolve binary path") << "\n";
    }

    // 3. Optional provider check (informational, not failures)
    static const std::vector<std::pair<std::string, bool>> providers = {
        {"ollama", false}, {"claude", false}, {"gemini", false}, {"codex", false}
    };
    int providers_found = 0;
    std::cout << "\n  " << term::bold(tr("Providers:")) << "\n";
    for (const auto& [name, _] : providers) {
        bool found = Process::available(name);
        if (found) ++providers_found;
        std::cout << "  " << (found ? term::icon_ok() : term::icon_warn())
                  << " " << name << " " << (found ? term::dim(tr("(available)")) : term::dim(tr("(not found, optional)"))) << "\n";
    }

    // 4. Shell setup (opt-in only)
    auto shell = shell_override.empty() ? detect_shell() : shell_override;
    auto config_path = shell_config_path(shell);

    if (shell_setup) {
        if (shell == "unknown" || config_path.empty()) {
            std::cout << "\n  " << term::icon_warn() << " " << tr("Unknown shell, skipping config") << "\n";
        } else {
            std::cout << "\n  " << term::bold(tr("Shell setup:")) << " " << shell << "\n";

            bool has_marker = has_config_entry(config_path, kMarkerBegin);
            if (has_marker && !force) {
                std::cout << "  " << term::icon_ok() << " " << tr("Shell config") << " " << term::dim(tr("(already configured)")) << "\n";
            } else {
                if (dry_run) {
                    std::cout << "  " << term::icon_info() << " " << tr("Would add EUXIS_HOME + PATH to") << " " << config_path.string() << "\n";
                } else {
                    if (has_marker && force) remove_shell_block(config_path);
                    write_shell_block(config_path, shell);
                    std::cout << "  " << term::icon_ok() << " " << tr("Added EUXIS_HOME + PATH to") << " " << config_path.string() << "\n";
                }
                ++actions;
            }

            if (!no_completions) {
                if (install_completions(ctx.euxis_home, shell, dry_run, force)) ++actions;
            }
        }
    } else {
        // Print next steps instead of modifying config
        std::cout << "\n  " << term::bold(tr("Next steps:")) << "\n";
        if (shell != "unknown" && !config_path.empty()) {
            std::cout << "  " << tr("Add to") << " " << config_path.string() << ":\n\n";
            if (shell == "fish") {
                std::cout << "    " << term::cyan("set -gx EUXIS_HOME \"$HOME/.euxis\"") << "\n";
                std::cout << "    " << term::cyan("fish_add_path -g \"$HOME/.euxis/bin\"") << "\n";
            } else {
                std::cout << "    " << term::cyan("export EUXIS_HOME=\"$HOME/.euxis\"") << "\n";
                std::cout << "    " << term::cyan("export PATH=\"$EUXIS_HOME/bin:$PATH\"") << "\n";
            }
            std::cout << "\n  " << tr("Or run:") << " " << term::cyan("euxis install --shell-setup --shell " + shell) << "\n";
        }
    }

    // 5. Summary
    std::cout << "\n";
    if (dry_run) {
        std::cout << term::bold(tr("Dry run:")) << " " << actions << " " << tr("action(s) would be performed") << "\n";
    } else if (actions == 0) {
        std::cout << term::green(tr("Already installed — nothing to do")) << "\n";
    } else {
        std::cout << term::green(std::to_string(actions) + " " + tr("action(s) completed")) << "\n";
        if (shell_setup && shell != "unknown") {
            std::cout << "\n" << tr("Restart your shell or run:") << "\n";
            std::cout << "  " << term::cyan("source " + config_path.string()) << "\n";
        }
    }

    return 0;
}

// --- update ---

int cmd_update(Context& ctx, const std::vector<std::string>& args) {
    bool dry_run = false;
    bool fetch = false;

    for (const auto& a : args) {
        if (a == "--help" || a == "-h") {
            std::cerr << tr("Refresh local metadata and registry.") << "\n\n"
                      << tr("Usage:") << "\n"
                      << "  euxis update [options]\n\n"
                      << tr("Options:") << "\n"
                      << "  --dry-run    " << tr("Show what would be done without modifying anything") << "\n"
                      << "  --fetch      " << tr("Also fetch latest git metadata (does not change code)") << "\n";
            return 0;
        }
        if (a == "--dry-run") dry_run = true;
        if (a == "--fetch")   fetch = true;
    }

    std::cout << term::bold(tr("Euxis Update")) << "\n\n";

    // 1. Optional git fetch (metadata only, not pull)
    if (fetch) {
        if (fs::exists(fs::path(ctx.euxis_home) / ".git")) {
            if (dry_run) {
                std::cout << "  " << term::icon_info() << " " << tr("Would run: git fetch") << "\n";
            } else {
                auto result = Process::run("git", {"-C", ctx.euxis_home, "fetch"});
                if (result.exit_code != 0) {
                    std::cerr << "  " << term::icon_warn() << " " << tr("git fetch failed (non-fatal)") << "\n";
                } else {
                    std::cout << "  " << term::icon_ok() << " " << tr("Git metadata fetched") << "\n";
                }
            }
        } else {
            std::cout << "  " << term::icon_info() << " " << tr("Not a git repo, skipping fetch") << "\n";
        }
    }

    // 2. Validate registry
    auto registry_json = fs::path(ctx.data_dir) / "agents" / "registry.json";
    auto registry_db   = fs::path(ctx.data_dir) / "agents" / "registry.db";
    if (fs::exists(registry_json)) {
        // Validate JSON parseable
        std::ifstream f(registry_json);
        try {
            auto j = nlohmann::json::parse(f);
            int agent_count = 0;
            if (j.contains("agents") && j["agents"].is_array()) {
                agent_count = static_cast<int>(j["agents"].size());
            }
            std::cout << "  " << term::icon_ok() << " " << tr("Registry:") << " " << agent_count << " " << tr("agents") << "\n";
        } catch (...) {
            std::cout << "  " << term::icon_fail() << " " << tr("Registry JSON is invalid") << "\n";
        }

        // Check index freshness
        if (fs::exists(registry_db)) {
            auto json_time = fs::last_write_time(registry_json);
            auto db_time   = fs::last_write_time(registry_db);
            if (json_time > db_time) {
                std::cout << "  " << term::icon_warn() << " " << tr("Registry index stale — run 'euxis update' to refresh") << "\n";
            } else {
                std::cout << "  " << term::icon_ok() << " " << tr("Registry index up to date") << "\n";
            }
        }
    } else {
        std::cout << "  " << term::icon_warn() << " " << tr("No registry.json found") << "\n";
    }

    // 3. Validate config files
    auto config_dir = fs::path(ctx.data_dir) / "config";
    if (fs::is_directory(config_dir)) {
        int configs = 0;
        for (const auto& entry : fs::directory_iterator(config_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") ++configs;
        }
        std::cout << "  " << term::icon_ok() << " " << tr("Config files:") << " " << configs << "\n";
    }

    // 4. Validate/install completions
    auto shell = detect_shell();
    if (shell != "unknown") {
        if (dry_run) {
            std::cout << "  " << term::icon_info() << " " << tr("Would validate completions for") << " " << shell << "\n";
        } else {
            install_completions(ctx.euxis_home, shell, false, false);
        }
    }

    // 5. Check playbook manifests
    auto playbook_dir = fs::path(ctx.data_dir) / "config" / "playbooks";
    if (fs::is_directory(playbook_dir)) {
        int playbooks = 0;
        for (const auto& entry : fs::directory_iterator(playbook_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") ++playbooks;
        }
        std::cout << "  " << term::icon_ok() << " " << tr("Playbooks:") << " " << playbooks << "\n";
    }

    std::cout << "\n";
    if (dry_run) {
        std::cout << term::bold(tr("Dry run complete")) << "\n";
    } else {
        std::cout << term::green(tr("Update complete")) << "\n";
    }
    return 0;
}

// --- upgrade ---

int cmd_upgrade(Context& ctx, const std::vector<std::string>& args) {
    bool dry_run = false;
    bool clean = false;

    for (const auto& a : args) {
        if (a == "--help" || a == "-h") {
            std::cerr << tr("Upgrade binary (pull + rebuild).") << "\n\n"
                      << tr("Usage:") << "\n"
                      << "  euxis upgrade [options]\n\n"
                      << tr("Options:") << "\n"
                      << "  --dry-run    " << tr("Show what would be done without modifying anything") << "\n"
                      << "  --clean      " << tr("Delete cmake-build/ before rebuilding") << "\n";
            return 0;
        }
        if (a == "--dry-run") dry_run = true;
        if (a == "--clean")   clean = true;
    }

    std::cout << term::bold(tr("Euxis Upgrade")) << "\n\n";

    // 1. Git pull
    if (!fs::exists(fs::path(ctx.euxis_home) / ".git")) {
        std::cerr << term::icon_fail() << " " << tr("EUXIS_HOME is not a git repository:") << " " << ctx.euxis_home << "\n";
        return 1;
    }

    if (dry_run) {
        std::cout << "  " << term::icon_info() << " " << tr("Would run: git pull --ff-only") << "\n";
    } else {
        std::cout << "  " << tr("Pulling latest changes...") << "\n";
        auto result = Process::run("git", {"-C", ctx.euxis_home, "pull", "--ff-only"});
        if (result.exit_code != 0) {
            std::cerr << "  " << term::icon_fail() << " " << tr("git pull failed:") << "\n"
                      << result.stderr_output << "\n";
            return 1;
        }
        std::cout << "  " << term::icon_ok() << " " << tr("Git pull complete") << "\n";
    }

    auto build_dir = fs::path(ctx.euxis_home) / "cmake-build";

    // 2. Clean build dir if requested
    if (clean) {
        if (dry_run) {
            std::cout << "  " << term::icon_info() << " " << tr("Would delete") << " " << build_dir.string() << "\n";
        } else if (fs::exists(build_dir)) {
            fs::remove_all(build_dir);
            std::cout << "  " << term::icon_ok() << " " << tr("Cleaned build directory") << "\n";
        }
    }

    // 3. CMake configure (only if cache doesn't exist)
    if (!fs::exists(build_dir / "CMakeCache.txt")) {
        if (dry_run) {
            std::cout << "  " << term::icon_info() << " " << tr("Would run: cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release") << "\n";
        } else {
            std::cout << "  " << tr("Configuring CMake...") << "\n";
            auto result = Process::run("cmake", {
                "-S", ctx.euxis_home,
                "-B", build_dir.string(),
                "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
            }, /*timeout_seconds=*/120);
            if (result.exit_code != 0) {
                std::cerr << "  " << term::icon_fail() << " " << tr("CMake configure failed") << "\n"
                          << result.stderr_output << "\n";
                return 1;
            }
            std::cout << "  " << term::icon_ok() << " " << tr("CMake configured") << "\n";
        }
    } else {
        std::cout << "  " << term::icon_ok() << " " << tr("CMake cache exists, skipping configure") << "\n";
    }

    // 4. CMake build
    if (dry_run) {
        std::cout << "  " << term::icon_info() << " " << tr("Would run: cmake --build cmake-build") << "\n";
    } else {
        std::cout << "  " << tr("Building...") << "\n";
        auto nproc_result = Process::run("nproc", {});
        std::string jobs = "4";
        if (nproc_result.exit_code == 0) {
            jobs = nproc_result.stdout_output;
            while (!jobs.empty() && (jobs.back() == '\n' || jobs.back() == '\r'))
                jobs.pop_back();
        }

        auto result = Process::run("cmake", {
            "--build", build_dir.string(), "-j", jobs
        }, /*timeout_seconds=*/600);
        if (result.exit_code != 0) {
            std::cerr << "  " << term::icon_fail() << " " << tr("Build failed") << "\n"
                      << result.stderr_output << "\n";
            return 1;
        }
        std::cout << "  " << term::icon_ok() << " " << tr("Build complete") << "\n";
    }

    // 5. Update symlink
    auto new_binary = build_dir / "cmd" / "cli" / "euxis-cli";
    auto bin_link = fs::path(ctx.euxis_home) / "bin" / "euxis";
    if (fs::exists(new_binary)) {
        if (dry_run) {
            std::cout << "  " << term::icon_info() << " " << tr("Would update symlink") << " " << bin_link.string() << "\n";
        } else {
            std::error_code ec;
            fs::create_directories(bin_link.parent_path());
            fs::remove(bin_link, ec);
            fs::create_symlink(fs::canonical(new_binary), bin_link, ec);
            if (ec) {
                std::cout << "  " << term::icon_fail() << " " << tr("Failed to update symlink:") << " " << ec.message() << "\n";
            } else {
                std::cout << "  " << term::icon_ok() << " " << tr("Updated symlink") << "\n";
            }
        }
    }

    std::cout << "\n";
    if (dry_run) {
        std::cout << term::bold(tr("Dry run complete")) << "\n";
    } else {
        std::cout << term::green(tr("Upgrade complete")) << "\n";
    }
    return 0;
}

// --- uninstall ---

int cmd_uninstall(Context& ctx, const std::vector<std::string>& args) {
    bool dry_run = false;
    bool purge = false;
    bool yes = false;

    for (const auto& a : args) {
        if (a == "--help" || a == "-h") {
            std::cerr << tr("Remove Euxis from this machine.") << "\n\n"
                      << tr("Usage:") << "\n"
                      << "  euxis uninstall [options]\n\n"
                      << tr("Options:") << "\n"
                      << "  --purge      " << tr("Also remove EUXIS_HOME and all data") << "\n"
                      << "  --dry-run    " << tr("Show what would be done without modifying anything") << "\n"
                      << "  --yes        " << tr("Skip confirmation for --purge") << "\n";
            return 0;
        }
        if (a == "--dry-run") dry_run = true;
        if (a == "--purge")   purge = true;
        if (a == "--yes")     yes = true;
    }

    std::cout << term::bold(tr("Euxis Uninstall")) << "\n\n";

    int actions = 0;

    // 1. Remove binary symlink
    auto bin_link = fs::path(ctx.euxis_home) / "bin" / "euxis";
    if (fs::is_symlink(bin_link) || fs::exists(bin_link)) {
        if (dry_run) {
            std::cout << "  " << term::icon_info() << " " << tr("Would remove") << " " << bin_link.string() << "\n";
        } else {
            fs::remove(bin_link);
            std::cout << "  " << term::icon_ok() << " " << tr("Removed") << " " << bin_link.string() << "\n";
        }
        ++actions;
    } else {
        std::cout << "  " << term::icon_ok() << " " << tr("Binary symlink already absent") << "\n";
    }

    // 2. Remove shell config entries (marker-delimited)
    auto shell = detect_shell();
    auto config_path = shell_config_path(shell);

    if (!config_path.empty() && fs::exists(config_path)) {
        bool has_marker = has_config_entry(config_path, kMarkerBegin);
        if (has_marker) {
            if (dry_run) {
                std::cout << "  " << term::icon_info() << " " << tr("Would remove EUXIS entries from") << " " << config_path.string() << "\n";
            } else {
                remove_shell_block(config_path);
                std::cout << "  " << term::icon_ok() << " " << tr("Removed EUXIS entries from") << " " << config_path.string() << "\n";
            }
            ++actions;
        }
    }

    // 3. Remove completion symlinks
    const char* home = std::getenv("HOME");
    if (home) {
        fs::path h(home);
        std::vector<fs::path> comp_paths = {
            h / ".config" / "fish" / "completions" / "euxis.fish",
            h / ".zsh" / "completions" / "_euxis",
        };
        for (const auto& cp : comp_paths) {
            if (fs::is_symlink(cp) || fs::exists(cp)) {
                if (dry_run) {
                    std::cout << "  " << term::icon_info() << " " << tr("Would remove") << " " << cp.string() << "\n";
                } else {
                    fs::remove(cp);
                    std::cout << "  " << term::icon_ok() << " " << tr("Removed") << " " << cp.string() << "\n";
                }
                ++actions;
            }
        }
    }

    // 4. Purge — remove EUXIS_HOME entirely
    if (purge) {
        if (!yes && !dry_run) {
            std::cout << "\n  " << term::yellow(tr("This will remove ALL data in") + " " + ctx.euxis_home) << "\n";
            std::cout << "  " << tr("Continue?") << " [y/N] ";
            std::string response;
            std::getline(std::cin, response);
            if (response != "y" && response != "Y") {
                std::cout << "  " << tr("Aborted.") << "\n";
                return 0;
            }
        }

        if (dry_run) {
            std::cout << "  " << term::icon_info() << " " << tr("Would remove") << " " << ctx.euxis_home << "\n";
        } else {
            std::error_code ec;
            fs::remove_all(ctx.euxis_home, ec);
            if (ec) {
                std::cerr << "  " << term::icon_fail() << " " << tr("Failed to remove:") << " " << ec.message() << "\n";
            } else {
                std::cout << "  " << term::icon_ok() << " " << tr("Removed") << " " << ctx.euxis_home << "\n";
            }
        }
        ++actions;
    }

    // 5. Summary
    std::cout << "\n";
    if (dry_run) {
        std::cout << term::bold(tr("Dry run:")) << " " << actions << " " << tr("action(s) would be performed") << "\n";
    } else if (actions == 0) {
        std::cout << term::green(tr("Nothing to remove")) << "\n";
    } else {
        std::cout << term::green(std::to_string(actions) + " " + tr("action(s) completed")) << "\n";
    }

    return 0;
}

// --- self ---

int cmd_self(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::cerr << tr("Installation introspection.") << "\n\n"
                  << tr("Usage:") << "\n"
                  << "  euxis self <subcommand>\n\n"
                  << tr("Subcommands:") << "\n"
                  << "  status    " << tr("Show installation status") << "\n"
                  << "  paths     " << tr("Print all known paths") << "\n"
                  << "  doctor    " << tr("Run installation diagnostics") << "\n"
                  << "  version   " << tr("Show version and build info") << "\n"
                  << "  where     " << tr("Print binary path") << "\n"
                  << "  repair    " << tr("Attempt automatic repair") << "\n";
        return args.empty() ? 2 : 0;
    }

    const auto& sub = args[0];
    std::vector<std::string> sub_args(args.begin() + 1, args.end());

    // Check for --json in sub_args
    bool json_out = ctx.json_output;
    for (const auto& a : sub_args) {
        if (a == "--json") json_out = true;
    }

    // --- self status ---
    if (sub == "status") {
        bool home_exists = fs::is_directory(ctx.euxis_home);
        auto bin_link = fs::path(ctx.euxis_home) / "bin" / "euxis";
        bool link_ok = fs::is_symlink(bin_link) || fs::exists(bin_link);
        auto shell = detect_shell();
        auto config_path = shell_config_path(shell);
        bool shell_ok = !config_path.empty() && has_config_entry(config_path, "EUXIS_HOME");
        bool comp_ok = false;
        const char* home = std::getenv("HOME");
        if (home) {
            if (shell == "fish")
                comp_ok = fs::exists(fs::path(home) / ".config" / "fish" / "completions" / "euxis.fish");
            else if (shell == "zsh")
                comp_ok = fs::exists(fs::path(home) / ".zsh" / "completions" / "_euxis");
            else if (shell == "bash")
                comp_ok = has_config_entry(config_path, "euxis.bash");
        }

        auto registry_path = fs::path(ctx.data_dir) / "agents" / "registry.json";
        int agents = 0;
        if (fs::exists(registry_path)) {
            std::ifstream f(registry_path);
            try {
                auto j = nlohmann::json::parse(f);
                if (j.contains("agents") && j["agents"].is_array()) {
                    agents = static_cast<int>(j["agents"].size());
                }
            } catch (...) {}
        }

        auto mode = detect_install_mode(ctx.euxis_home);

        if (json_out) {
            nlohmann::json j;
            j["version"] = "v0.0.4";
            j["euxis_home"] = ctx.euxis_home;
            j["euxis_home_exists"] = home_exists;
            j["binary_symlink"] = link_ok;
            j["shell"] = shell;
            j["shell_configured"] = shell_ok;
            j["completions_installed"] = comp_ok;
            j["agents"] = agents;
            j["install_mode"] = mode;
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << term::bold(tr("Euxis Status")) << "\n\n";
            std::cout << "  " << tr("Version:") << "       v0.0.4\n";
            std::cout << "  " << tr("Mode:") << "          " << mode << "\n";
            std::cout << "  " << tr("EUXIS_HOME:") << "    "
                      << (home_exists ? term::icon_ok() : term::icon_fail())
                      << " " << ctx.euxis_home << "\n";
            std::cout << "  " << tr("Binary:") << "        "
                      << (link_ok ? term::icon_ok() : term::icon_fail())
                      << " " << bin_link.string() << "\n";
            std::cout << "  " << tr("Shell:") << "         "
                      << (shell_ok ? term::icon_ok() : term::icon_warn())
                      << " " << shell << "\n";
            std::cout << "  " << tr("Completions:") << "   "
                      << (comp_ok ? term::icon_ok() : term::icon_warn())
                      << " " << (comp_ok ? tr("installed") : tr("not found")) << "\n";
            std::cout << "  " << tr("Agents:") << "        " << agents << "\n";
        }
        return 0;
    }

    // --- self paths ---
    if (sub == "paths") {
        if (json_out) {
            nlohmann::json j;
            j["euxis_home"] = ctx.euxis_home;
            j["binary"] = resolve_binary_path().string();
            j["data"] = ctx.data_dir;
            j["config"] = (fs::path(ctx.data_dir) / "config").string();
            j["completions"] = completions_source_dir(ctx.euxis_home).string();
            j["sessions"] = (fs::path(ctx.data_dir) / "runtime" / "sessions").string();
            j["verdicts"] = (fs::path(ctx.data_dir) / "runtime" / "verdicts").string();
            j["memory"] = (fs::path(ctx.data_dir) / "runtime" / "memory").string();
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << term::bold(tr("Euxis Paths")) << "\n\n";
            std::cout << "  " << term::cyan(tr("EUXIS_HOME:")) << "    " << ctx.euxis_home << "\n";
            std::cout << "  " << term::cyan(tr("Binary:")) << "        " << resolve_binary_path().string() << "\n";
            std::cout << "  " << term::cyan(tr("Data:")) << "          " << ctx.data_dir << "\n";
            std::cout << "  " << term::cyan(tr("Config:")) << "        " << (fs::path(ctx.data_dir) / "config").string() << "\n";
            std::cout << "  " << term::cyan(tr("Completions:")) << "   " << completions_source_dir(ctx.euxis_home).string() << "\n";
            std::cout << "  " << term::cyan(tr("Sessions:")) << "      " << (fs::path(ctx.data_dir) / "runtime" / "sessions").string() << "\n";
            std::cout << "  " << term::cyan(tr("Verdicts:")) << "      " << (fs::path(ctx.data_dir) / "runtime" / "verdicts").string() << "\n";
            std::cout << "  " << term::cyan(tr("Memory:")) << "        " << (fs::path(ctx.data_dir) / "runtime" / "memory").string() << "\n";
        }
        return 0;
    }

    // --- self doctor ---
    if (sub == "doctor") {
        return cmd_doctor(ctx, sub_args);
    }

    // --- self version ---
    if (sub == "version") {
        if (json_out) {
            nlohmann::json j;
            j["version"] = "v0.0.4";
            j["language"] = "C++23";
            j["protocol"] = "1.0";
            auto registry_path = fs::path(ctx.data_dir) / "agents" / "registry.json";
            if (fs::exists(registry_path)) {
                std::ifstream f(registry_path);
                try {
                    auto rj = nlohmann::json::parse(f);
                    if (rj.contains("version"))
                        j["registry_version"] = rj["version"].get<std::string>();
                } catch (...) {}
            }
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << "Euxis v0.0.4 (C++23)\n";
            auto registry_path = fs::path(ctx.data_dir) / "agents" / "registry.json";
            if (fs::exists(registry_path)) {
                std::ifstream f(registry_path);
                try {
                    auto j = nlohmann::json::parse(f);
                    if (j.contains("version"))
                        std::cout << tr("Registry:") << " " << j["version"].get<std::string>() << "\n";
                } catch (...) {}
            }
            std::cout << tr("Protocol:") << " 1.0\n";
        }
        return 0;
    }

    // --- self where ---
    if (sub == "where") {
        auto path = resolve_binary_path();
        if (!path.empty()) {
            std::cout << path.string() << "\n";
        } else {
            std::cerr << tr("Could not resolve binary path") << "\n";
            return 1;
        }
        return 0;
    }

    // --- self repair ---
    if (sub == "repair") {
        return cmd_fix(ctx, sub_args);
    }

    std::cerr << tr("Unknown self subcommand:") << " " << sub << "\n"
              << tr("Usage: euxis self <status|paths|doctor|version|where|repair>") << "\n";
    return 2;
}

} // namespace euxis::cli::cmd
