#include "euxis/cli/claim_verifiers.hpp"

#include "euxis/cli/process.hpp"
#include "euxis/cli/terminal.hpp"
#include "euxis/runtime/config.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>

namespace euxis::cli::claims {

namespace fs = std::filesystem;
namespace term = terminal;

// =====================================================================
//  CLAIM VERIFICATION ENGINE — Upgrade inferred claims to execution-backed
// =====================================================================

namespace {

// Files present in the repo being analysed (its root, not the cwd).
struct RepoFiles {
    std::string root;
    [[nodiscard]] auto has(const std::string& path) const -> bool {
        return root.empty() ? fs::exists(path) : fs::exists(fs::path(root) / path);
    }
    [[nodiscard]] auto any(std::initializer_list<const char*> paths) const -> bool {
        return std::ranges::any_of(paths, [this](const char* p) { return has(p); });
    }
};

// The project's `.euxis.yaml` `verify:` list, when it has one.
auto configured_verifiers(const std::string& repo_root) -> std::optional<VerifierList> {
    const fs::path root = repo_root.empty() ? fs::current_path() : fs::path(repo_root);
    auto cfg = euxis::runtime::resolve_config(root, {});
    if (!cfg) {
        // A config that does not load must not pass for "no verify list".
        std::cerr << "euxis: " << cfg.error().file.string() << ": " << cfg.error().message
                  << "; detecting verify commands instead\n";
        return std::nullopt;
    }
    if (cfg->verify.commands.empty())
        return std::nullopt;
    VerifierList verifiers;
    for (const auto& vc : cfg->verify.commands)
        verifiers.emplace_back(vc.name, vc.command);
    return verifiers;
}

void add_node_verifiers(const RepoFiles& f, VerifierList& v) {
    if (!f.has("package.json"))
        return;
    if (f.has("node_modules/.bin/jest")) {
        v.push_back({"jest", {"npx", "jest", "--passWithNoTests", "--silent"}});
    } else if (f.has("node_modules/.bin/vitest")) {
        v.push_back({"vitest", {"npx", "vitest", "run", "--reporter=verbose"}});
    } else {
        v.push_back({"npm-test", {"npm", "test", "--if-present"}});
    }
}

void add_python_verifiers(const RepoFiles& f, VerifierList& v) {
    if (f.any({"pyproject.toml", "setup.py", "requirements.txt"}) &&
        f.any({"pytest.ini", "pyproject.toml"})) {
        v.push_back({"pytest", {"python", "-m", "pytest", "-q", "--tb=no"}});
    }
}

void add_cmake_verifiers(const RepoFiles& f, VerifierList& v) {
    if (!f.has("CMakeLists.txt"))
        return;
    // Don't re-run our own build — look for a build dir
    const char* build_dir = f.has("cmake-build/Makefile") ? "cmake-build"
                            : f.has("build/Makefile")     ? "build"
                                                          : nullptr;
    if (build_dir != nullptr) {
        v.push_back({"cmake-build", {"cmake", "--build", build_dir, "--target", "all"}});
    }
}

// First three non-empty lines, space-joined.
auto summarize_output(const std::string& text) -> std::string {
    std::string summary;
    std::istringstream ss(text);
    std::string line;
    int line_count = 0;
    while (line_count < 3 && std::getline(ss, line)) {
        if (line.empty())
            continue;
        summary += line + " ";
        ++line_count;
    }
    return summary;
}

auto run_verifier(const std::string& name, const std::vector<std::string>& cmd, int timeout_s)
    -> VerificationResult {
    auto start = std::chrono::steady_clock::now();
    std::vector<std::string> args(cmd.begin() + 1, cmd.end());
    auto proc = Process::run(cmd[0], args, timeout_s);
    auto elapsed = std::chrono::steady_clock::now() - start;
    const bool ok = proc.success();
    return {
        .check_name = name,
        .command = cmd[0] + " " + (args.empty() ? "" : args[0]),
        .success = ok,
        .duration_ms = std::chrono::duration<double, std::milli>(elapsed).count(),
        .claims_upgraded = ok ? 3 : 0,  // each successful check upgrades ~3 inferred claims
        .output_summary = summarize_output(ok ? proc.stdout_output : proc.stderr_output),
    };
}

}  // namespace

auto detect_project_verifiers(const std::string& repo_root) -> VerifierList {
    // A project's own `verify:` commands (.euxis.yaml) replace detection: a
    // detected `npm test` can be a suite far longer than the agent budget.
    if (auto configured = configured_verifiers(repo_root))
        return std::move(*configured);

    const RepoFiles f{repo_root};
    VerifierList v;
    if (f.has("Cargo.toml")) {
        v.push_back({"cargo-check", {"cargo", "check", "--quiet"}});
        v.push_back({"cargo-test", {"cargo", "test", "--quiet", "--no-fail-fast"}});
    }
    add_node_verifiers(f, v);
    if (f.has("go.mod")) {
        v.push_back({"go-vet", {"go", "vet", "./..."}});
        v.push_back({"go-test", {"go", "test", "-count=1", "-short", "./..."}});
    }
    add_python_verifiers(f, v);
    add_cmake_verifiers(f, v);
    // Generic: check for Makefile
    if (v.empty() && f.has("Makefile"))
        v.push_back({"make-check", {"make", "check"}});
    return v;
}

auto run_claim_verifiers(const VerifierList& verifiers, int timeout_per_check)
    -> std::vector<VerificationResult> {
    std::vector<VerificationResult> results;
    for (const auto& [name, cmd] : verifiers) {
        if (!cmd.empty())
            results.push_back(run_verifier(name, cmd, timeout_per_check));
    }
    return results;
}

auto verifier_pillar(const std::string& name) -> std::string {
    auto has = [&name](std::string_view s) { return name.find(s) != std::string::npos; };
    if (has("build") || has("cmake") || has("cargo-check") || has("go-vet") || has("make"))
        return "build";
    if (has("test") || has("jest") || has("vitest") || has("pytest"))
        return "testing";
    return "";
}

namespace {

void report_verification(const VerificationResult& vr, std::ostream& tui) {
    if (vr.success) {
        tui << "    " << term::icon_ok() << " " << term::bold(vr.check_name) << " "
            << term::green("VERIFIED") << " " << term::dim(term::format_duration(vr.duration_ms))
            << "\n";
    } else {
        tui << "    " << term::icon_fail() << " " << term::bold(vr.check_name) << " "
            << term::red("FAILED") << " " << term::dim(vr.output_summary) << "\n";
    }
}

}  // namespace

auto run_claim_verification(int timeout_s, bool enabled, std::ostream& tui) -> ClaimVerification {
    ClaimVerification cv;
    if (!enabled) {
        tui << "\n"
            << term::dim(
                   "  Claim verification skipped: euxis does not run the scanned "
                   "repository's build or test commands unless --run-verifiers is given.")
            << "\n";
        return cv;
    }
    // Use current directory as repo root (euxis should be run from repo root)
    auto verifiers = detect_project_verifiers(fs::current_path().string());
    if (verifiers.empty())
        return cv;
    tui << "\n" << term::bold(term::cyan("  === CLAIM VERIFICATION ===")) << "\n";
    cv.results = run_claim_verifiers(verifiers, timeout_s);
    for (const auto& vr : cv.results) {
        if (vr.success)
            cv.claims_upgraded += vr.claims_upgraded;
        report_verification(vr, tui);
    }
    return cv;
}

}  // namespace euxis::cli::claims
