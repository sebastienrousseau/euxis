#include "euxis/cli/cmd/dev.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/terminal.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <chrono>

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;
namespace term = terminal;

} // namespace

// --- bench ---

int cmd_bench(Context& ctx, const std::vector<std::string>& args) {
    std::string target = args.empty() ? "all" : args[0];
    int iterations = 10;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--iterations" && i + 1 < args.size()) {
            iterations = std::stoi(args[++i]);
        }
    }

    std::cout << term::bold("Benchmark: ") << target << "\n"
              << "  Iterations: " << iterations << "\n\n";

    // Built-in benchmarks
    if (target == "all" || target == "registry") {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto path = fs::path(ctx.data_dir) / "agents" / "registry.json";
            if (fs::exists(path)) {
                std::ifstream f(path);
                [[maybe_unused]] auto j = nlohmann::json::parse(f);
            }
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start);
        std::cout << "  registry.json parse: "
                  << (elapsed.count() / iterations) << " us/iter\n";
    }

    if (target == "all" || target == "process") {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            Process::run("echo", {"hello"});
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start);
        std::cout << "  process spawn: "
                  << (elapsed.count() / iterations) << " us/iter\n";
    }

    if (target == "all" || target == "filesystem") {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            [[maybe_unused]] auto a = fs::exists(ctx.euxis_home);
            [[maybe_unused]] auto b = fs::is_directory(ctx.data_dir);
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start);
        std::cout << "  filesystem check: "
                  << (elapsed.count() / iterations) << " us/iter\n";
    }

    std::cout << "\n" << term::icon_ok() << " Benchmark complete\n";
    return 0;
}

// --- hooks ---

int cmd_hooks(Context& ctx, const std::vector<std::string>& args) {
    auto hooks_dir = fs::path(ctx.euxis_home) / "euxis-bin" / "hooks";
    auto git_hooks_dir = fs::path(ctx.euxis_home) / ".git" / "hooks";

    if (args.empty() || args[0] == "list") {
        std::cout << term::bold("Git Hooks") << "\n\n";
        if (fs::is_directory(hooks_dir)) {
            for (const auto& entry : fs::directory_iterator(hooks_dir)) {
                std::cout << "  " << entry.path().filename().string() << "\n";
            }
        } else {
            std::cout << "  No hooks directory\n";
        }
        return 0;
    }

    if (args[0] == "install") {
        if (!fs::is_directory(hooks_dir)) {
            std::cerr << "No hooks to install\n";
            return 1;
        }
        fs::create_directories(git_hooks_dir);
        int installed = 0;
        for (const auto& entry : fs::directory_iterator(hooks_dir)) {
            auto target = git_hooks_dir / entry.path().filename();
            fs::copy(entry.path(), target, fs::copy_options::overwrite_existing);
            fs::permissions(target, fs::perms::owner_exec | fs::perms::owner_read |
                                   fs::perms::owner_write, fs::perm_options::add);
            ++installed;
        }
        std::cout << term::icon_ok() << " Installed " << installed << " hook(s)\n";
        return 0;
    }

    std::cerr << "Usage: euxis hooks <list|install>\n";
    return 2;
}

// --- setup-shell-hooks ---

int cmd_setup_shell_hooks(Context& ctx, const std::vector<std::string>& args) {
    (void)args;
    const char* shell = std::getenv("SHELL");
    std::string shell_name = shell ? fs::path(shell).filename().string() : "unknown";

    std::cout << term::bold("Shell Hook Setup") << "\n"
              << "  Shell: " << shell_name << "\n\n";

    auto bin_path = fs::path(ctx.euxis_home) / "euxis-cpp" / "build" / "euxis-cli-cpp" / "euxis-cli";
    if (!fs::exists(bin_path)) {
        bin_path = fs::path(ctx.euxis_home) / "euxis-bin" / "euxis";
    }

    if (shell_name == "bash") {
        std::cout << "Add to ~/.bashrc:\n\n"
                  << "  export PATH=\"" << bin_path.parent_path().string() << ":$PATH\"\n"
                  << "  # Optional: source euxis completions\n";
    } else if (shell_name == "zsh") {
        std::cout << "Add to ~/.zshrc:\n\n"
                  << "  export PATH=\"" << bin_path.parent_path().string() << ":$PATH\"\n";
    } else if (shell_name == "fish") {
        std::cout << "Run:\n\n"
                  << "  fish_add_path " << bin_path.parent_path().string() << "\n";
    } else {
        std::cout << "Add to your shell config:\n"
                  << "  export PATH=\"" << bin_path.parent_path().string() << ":$PATH\"\n";
    }

    return 0;
}

// --- git-guard ---

int cmd_git_guard(Context& ctx, const std::vector<std::string>& args) {
    (void)ctx;
    (void)args;

    if (!Process::available("git")) {
        std::cerr << "git not found\n";
        return 1;
    }

    std::cout << term::bold("Git Guard") << "\n\n";
    int issues = 0;

    // Check for uncommitted changes
    auto status = Process::run("git", {"status", "--porcelain"});
    if (!status.stdout_output.empty()) {
        std::cout << "  " << term::icon_warn() << " Uncommitted changes detected\n";
        ++issues;
    } else {
        std::cout << "  " << term::icon_ok() << " Working tree clean\n";
    }

    // Check branch
    auto branch = Process::run("git", {"branch", "--show-current"});
    if (!branch.stdout_output.empty()) {
        auto name = branch.stdout_output;
        while (!name.empty() && (name.back() == '\n' || name.back() == '\r'))
            name.pop_back();
        std::cout << "  " << term::icon_info() << " Branch: " << name << "\n";
        if (name == "main" || name == "master") {
            std::cout << "  " << term::icon_warn() << " Working on protected branch\n";
            ++issues;
        }
    }

    // Check for large files
    auto diff = Process::run("git", {"diff", "--cached", "--stat"});
    if (!diff.stdout_output.empty() && ctx.verbose) {
        std::cout << "  Staged changes:\n" << diff.stdout_output;
    }

    if (issues == 0) {
        std::cout << "\n" << term::green("Git guard passed") << "\n";
    } else {
        std::cout << "\n" << term::yellow(std::to_string(issues) + " concern(s)") << "\n";
    }
    return 0;
}

// --- license-check ---

int cmd_license_check(Context& ctx, const std::vector<std::string>& args) {
    (void)args;

    std::cout << term::bold("License Check") << "\n\n";

    auto root = fs::path(ctx.euxis_home);
    int found = 0;
    int missing = 0;

    // Check for LICENSE file
    for (const auto& name : {"LICENSE", "LICENSE.md", "LICENSE.txt"}) {
        if (fs::exists(root / name)) {
            std::cout << "  " << term::icon_ok() << " " << name << "\n";
            ++found;
        }
    }
    if (found == 0) {
        std::cout << "  " << term::icon_fail() << " No LICENSE file found\n";
        ++missing;
    }

    // Check for license headers in source files
    auto cpp_dir = root / "euxis-cpp";
    if (fs::is_directory(cpp_dir)) {
        int checked = 0;
        int with_header = 0;
        for (const auto& entry : fs::recursive_directory_iterator(cpp_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
                ++checked;
                std::ifstream f(entry.path());
                std::string first_line;
                std::getline(f, first_line);
                if (first_line.find("license") != std::string::npos ||
                    first_line.find("License") != std::string::npos ||
                    first_line.find("//") != std::string::npos) {
                    ++with_header;
                }
            }
        }
        std::cout << "  " << term::icon_info() << " Source files: " << checked
                  << " checked, " << with_header << " with headers\n";
    }

    return missing > 0 ? 1 : 0;
}

// --- docs-test ---

int cmd_docs_test(Context& ctx, const std::vector<std::string>& args) {
    (void)args;

    std::cout << term::bold("Documentation Tests") << "\n\n";

    auto docs_dir = fs::path(ctx.euxis_home) / "docs";
    if (!fs::is_directory(docs_dir)) {
        std::cout << "  No docs/ directory\n";
        return 0;
    }

    int total = 0;
    int issues = 0;

    for (const auto& entry : fs::recursive_directory_iterator(docs_dir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".md" && ext != ".rst") continue;
        ++total;

        // Check for broken relative links
        std::ifstream f(entry.path());
        std::string line;
        int lineno = 0;
        while (std::getline(f, line)) {
            ++lineno;
            // Simple check for markdown links
            size_t pos = 0;
            while ((pos = line.find("](", pos)) != std::string::npos) {
                size_t end = line.find(')', pos + 2);
                if (end != std::string::npos) {
                    auto link = line.substr(pos + 2, end - pos - 2);
                    if (!link.starts_with("http") && !link.starts_with("#")) {
                        auto target = entry.path().parent_path() / link;
                        if (!fs::exists(target)) {
                            if (ctx.verbose) {
                                std::cout << "  " << term::icon_fail() << " "
                                          << entry.path().filename().string()
                                          << ":" << lineno << " -> " << link << "\n";
                            }
                            ++issues;
                        }
                    }
                }
                pos = end != std::string::npos ? end : pos + 1;
            }
        }
    }

    std::cout << "  Files checked: " << total << "\n"
              << "  Broken links:  " << issues << "\n\n";
    std::cout << (issues == 0 ? term::green("Docs OK") : term::yellow(std::to_string(issues) + " issue(s)")) << "\n";
    return issues > 0 ? 1 : 0;
}

// --- sync-docs ---

int cmd_sync_docs(Context& ctx, const std::vector<std::string>& /*args*/) {
    std::cout << term::bold("Sync Documentation") << "\n\n";

    auto docs_dir = fs::path(ctx.euxis_home) / "docs";
    auto output_dir = fs::path(ctx.euxis_home) / "euxis-data" / "docs";
    auto man_dir = output_dir / "man";

    if (!fs::is_directory(docs_dir)) {
        std::cout << "  " << term::icon_fail() << " No docs/ directory found\n";
        return 1;
    }

    // Create output directories
    std::error_code ec;
    fs::create_directories(output_dir, ec);
    fs::create_directories(man_dir, ec);

    // Copy all *.md files from docs/ to output dir
    int synced = 0;
    int man_pages = 0;
    bool has_pandoc = Process::available("pandoc");

    for (const auto& entry : fs::recursive_directory_iterator(docs_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".md") continue;

        auto relative = fs::relative(entry.path(), docs_dir);
        auto dest = output_dir / relative;

        // Create subdirectories if needed
        fs::create_directories(dest.parent_path(), ec);

        // Copy the markdown file
        fs::copy(entry.path(), dest, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
            ++synced;
            std::cout << "  " << term::icon_ok() << " " << relative.string() << "\n";
        } else {
            std::cout << "  " << term::icon_fail() << " " << relative.string()
                      << " " << term::dim("(" + ec.message() + ")") << "\n";
        }

        // Generate man page if pandoc is available
        if (has_pandoc && !ec) {
            auto stem = entry.path().stem().string();
            auto man_output = man_dir / (stem + ".1");
            auto result = Process::run("pandoc", {
                "-s", "-t", "man",
                "-o", man_output.string(),
                entry.path().string()
            });
            if (result.exit_code == 0) {
                ++man_pages;
            }
        }
    }

    // Summary
    std::cout << "\n";
    std::cout << "  Synced: " << synced << " file(s) -> "
              << output_dir.string() << "\n";
    if (has_pandoc) {
        std::cout << "  Man pages: " << man_pages << " generated -> "
                  << man_dir.string() << "\n";
    } else {
        std::cout << "  " << term::icon_info()
                  << " pandoc not found (man page generation skipped)\n";
    }

    std::cout << "\n" << (synced > 0 ? term::green("Sync complete") :
                          term::yellow("Nothing to sync")) << "\n";
    return 0;
}

// --- test-infra ---

int cmd_test_infra(Context& ctx, const std::vector<std::string>& args) {
    (void)args;

    std::cout << term::bold("Infrastructure Test Suite") << "\n\n";

    struct InfraTest {
        std::string name;
        std::string check;
    };

    std::vector<InfraTest> tests = {
        {"EUXIS_HOME exists", "dir:" + ctx.euxis_home},
        {"euxis-data exists", "dir:" + ctx.data_dir},
        {"registry.json", "file:" + ctx.data_dir + "/agents/registry.json"},
        {"git available", "cmd:git"},
        {"sqlite3 available", "cmd:sqlite3"},
    };

    int passed = 0;
    int failed = 0;

    for (const auto& t : tests) {
        bool ok = false;
        if (t.check.starts_with("dir:")) {
            ok = fs::is_directory(t.check.substr(4));
        } else if (t.check.starts_with("file:")) {
            ok = fs::exists(t.check.substr(5));
        } else if (t.check.starts_with("cmd:")) {
            ok = Process::available(t.check.substr(4));
        }

        std::cout << "  " << (ok ? term::icon_ok() : term::icon_fail())
                  << " " << t.name << "\n";
        if (ok) ++passed; else ++failed;
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}

} // namespace euxis::cli::cmd
