#include "euxis/cli/certification.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/terminal.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <unistd.h>

using euxis::cli::i18n::tr;

namespace euxis::cli::certification {
namespace detail {

namespace fs = std::filesystem;
namespace term = terminal;

// --- Utility helpers ---

auto check_file(const fs::path& root, const std::string& path) -> bool {
    return fs::exists(root / path);
}

auto full_path(const fs::path& root, const std::string& path) -> std::string {
    return (root / path).string();
}

auto read_file_content(const fs::path& path) -> std::string {
    try {
        std::ifstream f(path);
        return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    } catch (const std::exception&) { return {}; }
}

// =====================================================================
//  HARD GATES (4 by default, commit_signing conditional)
// =====================================================================

auto gate_commit_signing(const fs::path& target, int window, const std::string& since_ref,
                         bool strict) -> GateResult {
    GateResult g;
    g.name = "commit_signing";
    g.blocking = strict;  // only blocking in strict mode

    std::vector<std::string> git_args = {"-C", target.string(), "log"};
    if (!since_ref.empty()) {
        git_args.push_back(since_ref + "..HEAD");
    }
    git_args.push_back("--format=%H %G?");
    git_args.push_back("-n");
    git_args.push_back(std::to_string(window));

    auto proc = Process::run("git", git_args, 15);
    if (!proc.success()) {
        g.status = "fail";
        g.detail = "Could not read git log";
        g.data = {{"window", window}, {"signed", 0}, {"unsigned", 0}, {"status", "error"}};
        return g;
    }

    int signed_count = 0, unsigned_count = 0;
    std::istringstream ss(proc.stdout_output);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.size() < 3) continue;
        char sig = line.back();
        if (sig == 'G' || sig == 'U') ++signed_count;
        else ++unsigned_count;
    }

    int total = signed_count + unsigned_count;
    if (total == 0) {
        g.status = "partial";
        g.detail = "No commits in scope";
    } else if (unsigned_count == 0) {
        g.status = "pass";
        g.detail = std::to_string(signed_count) + "/" + std::to_string(total) + " signed";
    } else if (signed_count > 0) {
        g.status = "partial";
        g.detail = std::to_string(signed_count) + "/" + std::to_string(total) + " signed";
    } else {
        g.status = "fail";
        g.detail = "0/" + std::to_string(total) + " signed";
    }

    g.data = {{"window", window}, {"signed", signed_count},
              {"unsigned", unsigned_count}, {"status", g.status}};
    return g;
}

auto gate_unit_test_health(const fs::path& target) -> GateResult {
    GateResult g;
    g.name = "unit_test_health";
    g.blocking = true;

    // Detect test framework (reuse fleet.cpp logic)
    bool tests_detected = false;
    std::string test_framework;
    std::string test_cmd;
    std::vector<std::string> test_args;

    if (check_file(target, "CMakeLists.txt")) {
        // Look for cmake-build or build dir with test binary
        // Preferred names first (exact match), then substring fallback
        static const std::vector<std::string> preferred_names = {
            "euxis-cli-cpp_tests", "tests", "unit_tests", "all_tests"
        };
        for (const auto& build_dir : {"cmake-build", "build"}) {
            if (!check_file(target, build_dir)) continue;
            // Pass 1: preferred names (exact filename match)
            for (const auto& pref : preferred_names) {
                auto candidate = target / build_dir / "cmd" / "cli" / pref;
                if (fs::exists(candidate) && ::access(candidate.c_str(), X_OK) == 0) {
                    tests_detected = true;
                    test_framework = "cmake/gtest";
                    test_cmd = candidate.string();
                    break;
                }
                // Also check directly in build dir
                candidate = target / build_dir / pref;
                if (fs::exists(candidate) && ::access(candidate.c_str(), X_OK) == 0) {
                    tests_detected = true;
                    test_framework = "cmake/gtest";
                    test_cmd = candidate.string();
                    break;
                }
            }
            if (tests_detected) break;
            // Pass 2: substring fallback (recursive scan)
            try {
                for (const auto& entry : fs::recursive_directory_iterator(
                         target / build_dir, fs::directory_options::skip_permission_denied)) {
                    if (entry.is_regular_file()) {
                        auto fname = entry.path().filename().string();
                        if (fname.find("test") != std::string::npos &&
                            ::access(entry.path().c_str(), X_OK) == 0) {
                            tests_detected = true;
                            test_framework = "cmake/gtest";
                            test_cmd = entry.path().string();
                            break;
                        }
                    }
                }
            } catch (const std::exception&) {}
            if (tests_detected) break;
        }
    }
    if (!tests_detected && check_file(target, "Cargo.toml")) {
        tests_detected = true;
        test_framework = "cargo";
        test_cmd = "cargo";
        test_args = {"test", "--manifest-path", full_path(target, "Cargo.toml")};
    }
    if (!tests_detected && check_file(target, "package.json")) {
        if (check_file(target, "node_modules/.bin/jest") ||
            check_file(target, "node_modules/.bin/vitest")) {
            tests_detected = true;
            test_framework = check_file(target, "node_modules/.bin/jest") ? "jest" : "vitest";
            test_cmd = "npx";
            test_args = {test_framework};
        }
    }
    if (!tests_detected && check_file(target, "go.mod")) {
        tests_detected = true;
        test_framework = "go";
        test_cmd = "go";
        test_args = {"test", "./..."};
    }
    if (!tests_detected && (check_file(target, "pytest.ini") || check_file(target, "pyproject.toml"))) {
        tests_detected = true;
        test_framework = "pytest";
        test_cmd = "pytest";
    }

    if (!tests_detected) {
        g.status = "fail";
        g.detail = "No test framework detected";
        g.data = {{"passed", 0}, {"failed", 0}, {"pass_rate", 0.0},
                  {"threshold", 0.95}, {"status", "no_tests"}};
        return g;
    }

    // Run tests
    ProcessResult proc;
    if (test_framework == "cmake/gtest") {
        proc = Process::run(test_cmd, test_args, 300);
    } else {
        proc = Process::run(test_cmd, test_args, 300);
    }

    // Parse gtest output for pass/fail counts
    int passed = 0, failed = 0;
    auto& output = proc.stdout_output.empty() ? proc.stderr_output : proc.stdout_output;
    std::istringstream out_ss(output);
    std::string line;
    while (std::getline(out_ss, line)) {
        if (line.find("[  PASSED  ]") != std::string::npos) {
            auto pos = line.find("]");
            if (pos != std::string::npos) {
                try { passed = std::stoi(line.substr(pos + 1)); } catch (const std::exception&) {}
            }
        }
        if (line.find("[  FAILED  ]") != std::string::npos) {
            auto pos = line.find("]");
            if (pos != std::string::npos) {
                try { failed = std::stoi(line.substr(pos + 1)); } catch (const std::exception&) {}
            }
        }
    }

    // Fallback: use exit code if no gtest output parsed
    if (passed == 0 && failed == 0) {
        if (proc.success()) {
            passed = 1; // At minimum something passed
            g.status = "partial";
            g.detail = "Tests passed (count unknown) — " + test_framework;
        } else {
            failed = 1;
            g.status = "fail";
            g.detail = "Tests failed — " + test_framework;
        }
    } else {
        int total = passed + failed;
        double rate = total > 0 ? static_cast<double>(passed) / total : 0.0;
        if (rate >= 0.95) {
            g.status = "pass";
            g.detail = std::to_string(passed) + "/" + std::to_string(total) +
                       " passed (" + std::to_string(static_cast<int>(rate * 100)) + "%, threshold 95%)";
        } else {
            g.status = "fail";
            g.detail = std::to_string(passed) + "/" + std::to_string(total) +
                       " passed (" + std::to_string(static_cast<int>(rate * 100)) + "%, threshold 95%)";
        }
    }

    double pass_rate = (passed + failed > 0) ? static_cast<double>(passed) / (passed + failed) : 0.0;
    g.data = {{"passed", passed}, {"failed", failed}, {"pass_rate", pass_rate},
              {"threshold", 0.95}, {"framework", test_framework}, {"status", g.status}};
    return g;
}

auto gate_build_integrity(const fs::path& target) -> GateResult {
    GateResult g;
    g.name = "build_integrity";
    g.blocking = true;

    std::string cmd;
    std::vector<std::string> args;

    if (check_file(target, "CMakeLists.txt")) {
        auto build_dir = check_file(target, "cmake-build") ? "cmake-build" : "build";
        cmd = "cmake";
        args = {"--build", full_path(target, build_dir), "-j"};
        // Append nproc
        auto nproc = Process::shell("nproc 2>/dev/null || echo 4", 5);
        std::string cores = "4";
        if (nproc.success()) {
            cores = nproc.stdout_output;
            while (!cores.empty() && (cores.back() == '\n' || cores.back() == '\r'))
                cores.pop_back();
        }
        args.back() += cores;
    } else if (check_file(target, "Cargo.toml")) {
        cmd = "cargo";
        args = {"build", "--manifest-path", full_path(target, "Cargo.toml")};
    } else if (check_file(target, "package.json")) {
        cmd = "npm";
        args = {"run", "build", "--prefix", target.string()};
    } else if (check_file(target, "go.mod")) {
        cmd = "go";
        args = {"build", "./..."};
    } else {
        g.status = "partial";
        g.detail = "No recognized build system";
        g.data = {{"status", "no_build_system"}, {"command", ""}};
        return g;
    }

    auto proc = Process::run(cmd, args, 300);
    g.status = proc.success() ? "pass" : "fail";
    g.detail = proc.success() ? (cmd + " build OK") : (cmd + " build failed");
    g.data = {{"status", g.status}, {"command", cmd + " " + (args.empty() ? "" : args[0])}};
    return g;
}

auto gate_docs_accuracy(const fs::path& target,
                        const std::vector<std::string>& known_commands) -> GateResult {
    GateResult g;
    g.name = "documentation_accuracy";
    g.blocking = false;  // non-blocking by default; --strict makes it blocking

    bool has_readme = check_file(target, "README.md") || check_file(target, "readme.md");

    if (!has_readme) {
        g.status = "fail";
        g.detail = "No README.md found";
        g.data = {{"status", "no_readme"}, {"checked_surfaces", nlohmann::json::array()},
                  {"stale_examples", 0}};
        return g;
    }

    // Read README and check for command references
    std::string readme_content;
    if (check_file(target, "README.md"))
        readme_content = read_file_content(target / "README.md");
    else
        readme_content = read_file_content(target / "readme.md");

    std::vector<std::string> checked_surfaces;
    int stale_count = 0;

    // Check if known commands appear in README
    for (const auto& cmd : known_commands) {
        if (readme_content.find(cmd) != std::string::npos) {
            checked_surfaces.push_back(cmd);
        }
    }

    // Check for stale command references in docs
    auto check_stale = [&](const fs::path& doc_path) {
        if (!fs::exists(doc_path)) return;
        auto content = read_file_content(doc_path);
        // Look for "euxis <word>" patterns and check against known commands
        std::istringstream ss(content);
        std::string token;
        bool after_euxis = false;
        while (ss >> token) {
            if (after_euxis) {
                // Strip markdown/punctuation
                while (!token.empty() && !std::isalnum(static_cast<unsigned char>(token.back())))
                    token.pop_back();
                if (!token.empty() && token[0] != '-' && token[0] != '<') {
                    bool found = false;
                    for (const auto& cmd : known_commands) {
                        if (cmd == token) { found = true; break; }
                    }
                    if (!found && token.size() > 1) ++stale_count;
                }
                after_euxis = false;
            }
            if (token == "euxis" || token == "`euxis") after_euxis = true;
        }
    };

    check_stale(target / "README.md");
    // Also scan docs/ if present
    if (check_file(target, "docs")) {
        try {
            for (const auto& entry : fs::recursive_directory_iterator(
                     target / "docs", fs::directory_options::skip_permission_denied)) {
                if (entry.is_regular_file() && entry.path().extension() == ".md") {
                    check_stale(entry.path());
                }
            }
        } catch (const std::exception&) {}
    }

    if (stale_count == 0 && !checked_surfaces.empty()) {
        g.status = "pass";
        g.detail = "README present, " + std::to_string(checked_surfaces.size()) +
                   " commands documented, no stale refs";
    } else if (stale_count > 0) {
        g.status = "partial";
        g.detail = std::to_string(stale_count) + " potentially stale command references";
    } else {
        g.status = "partial";
        g.detail = "README present but no command references found";
    }

    g.data = {{"status", g.status},
              {"checked_surfaces", checked_surfaces},
              {"stale_examples", stale_count}};
    return g;
}

auto gate_security_critical(const fs::path& target) -> GateResult {
    GateResult g;
    g.name = "security_critical";
    g.blocking = true;  // blocking on fail (not on partial)

    nlohmann::json security_data;
    bool secrets_found = false;
    std::string dep_audit_status = "not_run";
    std::string build_flags_status = "not_checked";

    // 1. Secret scan
    {
        bool has_env = check_file(target, ".env") || check_file(target, ".env.local");
        bool env_gitignored = false;

        if (check_file(target, ".gitignore")) {
            auto content = read_file_content(target / ".gitignore");
            if (content.find(".env") != std::string::npos) env_gitignored = true;
        }

        if (has_env && !env_gitignored) {
            secrets_found = true;
            security_data["secret_scan"] = "fail";
        } else {
            security_data["secret_scan"] = "pass";
        }
    }

    // 2. Dependency audit
    {
        bool audit_ran = false;
        if (check_file(target, "package-lock.json") && Process::available("npm")) {
            auto proc = Process::run("npm", {"audit", "--json"}, 30);
            audit_ran = true;
            dep_audit_status = proc.success() ? "pass" : "fail";
        }
        if (check_file(target, "Cargo.lock") && Process::available("cargo")) {
            auto which = Process::shell("cargo audit --version 2>/dev/null", 5);
            if (which.success()) {
                auto proc = Process::run("cargo", {"audit", "--quiet"}, 30);
                audit_ran = true;
                dep_audit_status = proc.success() ? "pass" : "fail";
            }
        }
        if (!audit_ran && check_file(target, "CMakeLists.txt")) {
            // Check for dependency management
            bool has_fetch = false;
            auto content = read_file_content(target / "CMakeLists.txt");
            if (content.find("FetchContent") != std::string::npos) has_fetch = true;
            bool has_vcpkg = check_file(target, "vcpkg.json");
            bool has_conan = check_file(target, "conanfile.txt") || check_file(target, "conanfile.py");
            if (has_fetch || has_vcpkg || has_conan) {
                dep_audit_status = "pass";
                audit_ran = true;
            }
        }
        if (!audit_ran) dep_audit_status = "unavailable";
        security_data["dependency_audit"] = dep_audit_status;
    }

    // 3. Build flags
    if (check_file(target, "CMakeLists.txt")) {
        auto content = read_file_content(target / "CMakeLists.txt");
        bool has_unsafe = false;
        if (content.find("-DNDEBUG") != std::string::npos && content.find("Release") == std::string::npos) {
            has_unsafe = true;
        }
        build_flags_status = has_unsafe ? "fail" : "pass";
    }
    security_data["build_flags"] = build_flags_status;

    // Overall status
    if (secrets_found) {
        g.status = "fail";
        g.detail = "Exposed secrets detected (.env not gitignored)";
    } else if (dep_audit_status == "fail") {
        g.status = "fail";
        g.detail = "Dependency audit found vulnerabilities";
    } else if (dep_audit_status == "unavailable") {
        g.status = "partial";
        g.detail = "Dependency audit unavailable";
        g.blocking = false;  // partial is not blocking
    } else {
        g.status = "pass";
        g.detail = "No secrets exposed, dependencies managed";
    }

    security_data["status"] = g.status;
    g.data = security_data;
    return g;
}

// =====================================================================
//  QUALITY RISK ANALYSIS (not a gate — feeds into domains)
// =====================================================================

auto analyze_quality_risk(const fs::path& target) -> QualityRisk {
    QualityRisk qr;
    qr.status = "pass";

    // Scan source files for complexity indicators
    std::vector<std::string> source_extensions = {".cpp", ".hpp", ".c", ".h", ".py",
                                                   ".js", ".ts", ".go", ".rs", ".java"};
    int files_scanned = 0;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(
                 target, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            auto path_str = entry.path().string();
            if (path_str.find(".git") != std::string::npos) continue;
            if (path_str.find("node_modules") != std::string::npos) continue;
            if (path_str.find("vendor") != std::string::npos) continue;
            if (path_str.find("cmake-build") != std::string::npos) continue;
            if (path_str.find("build/") != std::string::npos) continue;

            auto ext = entry.path().extension().string();
            bool is_source = false;
            for (const auto& se : source_extensions) {
                if (ext == se) { is_source = true; break; }
            }
            if (!is_source) continue;

            // Count lines
            try {
                std::ifstream f(entry.path());
                int line_count = 0;
                std::string line;
                while (std::getline(f, line)) ++line_count;

                if (line_count > 1000) {
                    qr.high_complexity_files.push_back(
                        entry.path().lexically_relative(target).string() +
                        " (" + std::to_string(line_count) + " lines)");
                    qr.status = "gaps";
                }
            } catch (const std::exception&) {}

            ++files_scanned;
            if (files_scanned >= 500) break; // Bounded
        }
    } catch (const std::exception&) {}

    qr.data = {{"status", qr.status},
               {"high_complexity_files", qr.high_complexity_files},
               {"hotspots", qr.hotspots},
               {"files_scanned", files_scanned}};
    return qr;
}

// =====================================================================
//  EVIDENCE COLLECTION
// =====================================================================

auto collect_evidence(const fs::path& target,
                      const std::vector<GateResult>& gates,
                      const QualityRisk& qr) -> std::vector<Evidence> {
    std::vector<Evidence> ev;
    int id_counter = 0;
    auto next_id = [&]() { return "ev-" + std::to_string(++id_counter); };

    // File-based evidence
    auto check_and_add = [&](const std::string& file, const std::string& domain,
                              const std::string& summary) {
        if (check_file(target, file)) {
            ev.push_back({next_id(), domain, "file", "pass", false, summary});
        }
    };

    // Governance
    check_and_add("CONTRIBUTING.md", "Governance & Organizational Controls", "CONTRIBUTING.md present");
    if (check_file(target, ".github/CODEOWNERS") || check_file(target, "CODEOWNERS"))
        ev.push_back({next_id(), "Governance & Organizational Controls", "file", "pass", false, "CODEOWNERS defined"});

    // Legal
    check_and_add("LICENSE", "Legal & Regulatory Compliance", "LICENSE file present");
    check_and_add("LICENSE.md", "Legal & Regulatory Compliance", "LICENSE file present");
    check_and_add("NOTICE", "Legal & Regulatory Compliance", "NOTICE file present");

    // SDLC
    check_and_add(".gitignore", "Secure Software Development Lifecycle", ".gitignore configured");
    check_and_add(".github/workflows", "Secure Software Development Lifecycle", "CI workflows present");
    check_and_add(".gitlab-ci.yml", "Secure Software Development Lifecycle", "GitLab CI present");
    check_and_add("SECURITY.md", "Secure Software Development Lifecycle", "Security policy defined");

    // IAM
    if (check_file(target, ".gitignore")) {
        auto content = read_file_content(target / ".gitignore");
        if (content.find(".env") != std::string::npos || content.find("credentials") != std::string::npos) {
            ev.push_back({next_id(), "Identity & Access Management", "file", "pass", false,
                          "Secrets excluded via .gitignore"});
        }
    }

    // Infrastructure
    check_and_add("Dockerfile", "Infrastructure & Cloud Security", "Dockerfile present");
    check_and_add("docker-compose.yml", "Infrastructure & Cloud Security", "Docker Compose present");
    check_and_add("docker-compose.yaml", "Infrastructure & Cloud Security", "Docker Compose present");

    // Application Security — from gate results
    for (const auto& gate : gates) {
        if (gate.name == "security_critical") {
            std::string domain = "Application Security";
            ev.push_back({next_id(), domain, "builtin", gate.status, true,
                          "Security gate: " + gate.detail});
        }
    }

    // Vulnerability Management
    check_and_add("package-lock.json", "Vulnerability & Patch Management", "Package lockfile present");
    check_and_add("Cargo.lock", "Vulnerability & Patch Management", "Cargo lockfile present");
    check_and_add("go.sum", "Vulnerability & Patch Management", "Go sum file present");
    check_and_add("poetry.lock", "Vulnerability & Patch Management", "Poetry lockfile present");

    // Logging & Monitoring
    if (check_file(target, "data/runtime/metrics") || check_file(target, "metrics"))
        ev.push_back({next_id(), "Logging, Monitoring & Detection", "file", "pass", false, "Metrics directory present"});
    if (check_file(target, "data/gateway/audit"))
        ev.push_back({next_id(), "Logging, Monitoring & Detection", "file", "pass", false, "Audit log directory present"});

    // Incident Response
    check_and_add("SECURITY.md", "Incident Response", "Security policy with contact info");

    // Performance
    if (check_file(target, "data/config/targets.json"))
        ev.push_back({next_id(), "Performance & Reliability", "artifact", "pass", false, "SLA targets defined"});

    // Software QA — from gate results
    for (const auto& gate : gates) {
        if (gate.name == "unit_test_health") {
            ev.push_back({next_id(), "Software Quality Assurance", "builtin",
                          gate.status, true, "Test gate: " + gate.detail});
        }
        if (gate.name == "build_integrity") {
            ev.push_back({next_id(), "Software Quality Assurance", "builtin",
                          gate.status, true, "Build gate: " + gate.detail});
        }
    }
    // Quality risk feeds into SQA
    if (!qr.high_complexity_files.empty()) {
        ev.push_back({next_id(), "Software Quality Assurance", "builtin", "partial", false,
                      std::to_string(qr.high_complexity_files.size()) + " high-complexity files detected"});
    }

    // Change Management — from commit signing gate
    for (const auto& gate : gates) {
        if (gate.name == "commit_signing") {
            ev.push_back({next_id(), "Change Management", "git", gate.status, true,
                          "Commit signing: " + gate.detail});
        }
    }
    check_and_add("CHANGELOG.md", "Change Management", "CHANGELOG present");
    check_and_add("CHANGELOG", "Change Management", "CHANGELOG present");

    // Supply Chain
    check_and_add("vcpkg.json", "Supply Chain & Third-Party Security", "vcpkg manifest present");
    check_and_add("package-lock.json", "Supply Chain & Third-Party Security", "npm lockfile present");
    check_and_add("Cargo.lock", "Supply Chain & Third-Party Security", "Cargo lockfile present");

    // Documentation
    check_and_add("README.md", "Documentation & Audit Evidence", "README present");
    if (check_file(target, "docs"))
        ev.push_back({next_id(), "Documentation & Audit Evidence", "file", "pass", false, "docs/ directory present"});
    for (const auto& gate : gates) {
        if (gate.name == "documentation_accuracy") {
            ev.push_back({next_id(), "Documentation & Audit Evidence", "builtin",
                          gate.status, true, "Docs gate: " + gate.detail});
        }
    }

    // User Protection
    if (check_file(target, "src/pii_filter.cpp") || check_file(target, "apps/cli/src/pii_filter.cpp"))
        ev.push_back({next_id(), "User Protection & Safety", "file", "pass", false, "PII filter present"});

    // Training
    check_and_add("docs/guides", "Training & Awareness", "Guides directory present");
    check_and_add("ONBOARDING.md", "Training & Awareness", "Onboarding docs present");

    // Euxis artifacts as evidence
    if (check_file(target, "data/runtime/sessions/latest_verdict.json"))
        ev.push_back({next_id(), "Software Quality Assurance", "artifact", "pass", true,
                      "Recent Euxis verdict artifact present"});
    if (check_file(target, "data/config/router.json"))
        ev.push_back({next_id(), "Performance & Reliability", "artifact", "pass", false,
                      "Model routing configuration present"});

    // Business Continuity
    check_and_add("BACKUP.md", "Business Continuity & Disaster Recovery", "Backup documentation present");
    check_and_add("docs/disaster-recovery.md", "Business Continuity & Disaster Recovery", "DR docs present");

    // Data Security
    check_and_add("PRIVACY.md", "Data Security & Privacy", "Privacy policy present");

    return ev;
}

// =====================================================================
//  DOMAIN EVALUATOR
// =====================================================================

auto evaluate_domains(std::vector<DomainDefinition>& definitions,
                      const std::vector<Evidence>& evidence) -> std::vector<DomainResult> {
    std::vector<DomainResult> results;

    for (auto& def : definitions) {
        DomainResult dr;
        dr.name = def.name;
        dr.critical = def.critical;

        // Count evidence for this domain
        int ev_count = 0;
        for (const auto& e : evidence) {
            if (e.domain == def.name) ++ev_count;
        }
        dr.evidence_count = ev_count;

        // Check required signals
        int required_met = 0;
        for (const auto& sig : def.required_signals) {
            bool found = false;
            for (const auto& e : evidence) {
                if (e.domain == def.name &&
                    (e.summary.find(sig) != std::string::npos || e.id.find(sig) != std::string::npos)) {
                    if (e.status == "pass" || e.status == "partial") {
                        found = true;
                        break;
                    }
                }
            }
            // Also check by evidence count — if we have any evidence for this domain
            // and the signal is generic enough
            if (!found && ev_count > 0) {
                // Check if signal name matches any evidence summary loosely
                for (const auto& e : evidence) {
                    if (e.domain == def.name && (e.status == "pass" || e.status == "partial")) {
                        // Loose match: signal word appears in summary
                        std::string lower_sig = sig;
                        std::string lower_sum = e.summary;
                        std::transform(lower_sig.begin(), lower_sig.end(), lower_sig.begin(), ::tolower);
                        std::transform(lower_sum.begin(), lower_sum.end(), lower_sum.begin(), ::tolower);
                        if (lower_sum.find(lower_sig) != std::string::npos) {
                            found = true;
                            break;
                        }
                    }
                }
            }
            if (found) {
                ++required_met;
                dr.findings.push_back(sig + ": covered");
            } else {
                dr.missing_controls.push_back(sig);
            }
        }

        // Coverage
        if (def.required_signals.empty()) {
            dr.coverage = ev_count > 0 ? 1.0 : 0.0;
        } else {
            dr.coverage = static_cast<double>(required_met) / static_cast<double>(def.required_signals.size());
        }

        // Status determination
        if (dr.coverage >= 1.0) {
            dr.status = "verified";
        } else if (dr.coverage >= 0.5) {
            dr.status = "gaps";
        } else if (dr.coverage == 0.0 && def.critical) {
            dr.status = "blocked";
        } else {
            dr.status = "inconclusive";
        }

        results.push_back(std::move(dr));
    }

    return results;
}

// =====================================================================
//  FINAL STATUS COMPUTATION
// =====================================================================

void finalize_result(RunResult& result) {
    // Check gates
    bool any_critical_gate_fail = false;
    int gate_pass = 0, gate_total = 0;
    int skipped_count = 0;

    for (const auto& g : result.gates) {
        if (g.status == "skipped") {
            ++skipped_count;
            result.skipped_gates.push_back(g.name);
            continue;
        }
        ++gate_total;
        if (g.status == "pass") ++gate_pass;
        if (g.status == "fail" && g.blocking) {
            any_critical_gate_fail = true;
            result.blocking_issues.push_back("Gate '" + g.name + "' failed: " + g.detail);
        }
    }

    // Check domains
    bool any_critical_domain_blocked = false;
    int domain_inconclusive = 0;
    double coverage_sum = 0.0;
    double critical_coverage_sum = 0.0;
    int critical_count = 0;

    for (const auto& d : result.domains) {
        coverage_sum += d.coverage;
        if (d.critical) {
            critical_coverage_sum += d.coverage;
            ++critical_count;
            if (d.status == "blocked") {
                any_critical_domain_blocked = true;
                result.blocking_issues.push_back("Critical domain '" + d.name + "' blocked");
            }
        }
        if (d.status == "inconclusive") ++domain_inconclusive;
        if (d.status == "gaps") {
            for (const auto& mc : d.missing_controls) {
                result.major_gaps.push_back(d.name + ": missing " + mc);
            }
        }
    }

    int total_domains = static_cast<int>(result.domains.size());
    double avg_coverage = total_domains > 0 ? coverage_sum / total_domains : 0.0;

    // Evidence stats
    int exec_backed = 0;
    for (const auto& e : result.evidence) {
        if (e.execution_backed) ++exec_backed;
    }
    int total_evidence = static_cast<int>(result.evidence.size());
    double exec_ratio = total_evidence > 0 ? static_cast<double>(exec_backed) / total_evidence : 0.0;

    // Status determination
    if (any_critical_gate_fail || any_critical_domain_blocked) {
        result.status = "BLOCKED";
        result.exit_code = 1;
    } else if (skipped_count > 2 || total_evidence < 5 ||
               domain_inconclusive > total_domains / 2) {
        result.status = "INCONCLUSIVE";
        result.exit_code = 1;
    } else if (avg_coverage > 0.75 && result.major_gaps.size() <= 3) {
        result.status = "READY";
        result.exit_code = 0;
    } else {
        result.status = "READY WITH GAPS";
        result.exit_code = 0;
    }

    // Confidence: gate pass (40) + domain coverage (30) + exec evidence (20) + critical coverage (10)
    double gate_score = gate_total > 0 ? (static_cast<double>(gate_pass) / gate_total) * 40.0 : 0.0;
    double domain_score = avg_coverage * 30.0;
    double exec_score = exec_ratio * 20.0;
    double critical_score = critical_count > 0 ? (critical_coverage_sum / critical_count) * 10.0 : 10.0;
    result.confidence = static_cast<int>(gate_score + domain_score + exec_score + critical_score);
    if (result.confidence > 100) result.confidence = 100;

    // Generate recommended actions
    for (const auto& d : result.domains) {
        for (const auto& mc : d.missing_controls) {
            if (mc.find("CONTRIBUTING") != std::string::npos)
                result.recommended_actions.push_back("Add CONTRIBUTING.md for governance coverage");
            else if (mc.find("SECURITY") != std::string::npos)
                result.recommended_actions.push_back("Create SECURITY.md with vulnerability disclosure process");
            else if (mc.find("training") != std::string::npos || mc.find("onboarding") != std::string::npos)
                result.recommended_actions.push_back("Add training/onboarding documentation");
            else if (mc.find("LICENSE") != std::string::npos)
                result.recommended_actions.push_back("Add LICENSE file");
        }
    }
    // Deduplicate
    std::sort(result.recommended_actions.begin(), result.recommended_actions.end());
    result.recommended_actions.erase(
        std::unique(result.recommended_actions.begin(), result.recommended_actions.end()),
        result.recommended_actions.end());

    // Summary
    int verified = 0, gaps = 0, blocked = 0, inconclusive = 0;
    for (const auto& d : result.domains) {
        if (d.status == "verified") ++verified;
        else if (d.status == "gaps") ++gaps;
        else if (d.status == "blocked") ++blocked;
        else ++inconclusive;
    }
    result.readiness_summary = result.status + " — " +
        std::to_string(verified) + " verified, " +
        std::to_string(gaps) + " with gaps, " +
        std::to_string(blocked) + " blocked, " +
        std::to_string(inconclusive) + " inconclusive";

    if (!result.skipped_gates.empty()) {
        result.readiness_summary += " (readiness reduced: " +
            std::to_string(result.skipped_gates.size()) + " gate(s) skipped)";
    }
}

} // namespace detail

// =====================================================================
//  PUBLIC API
// =====================================================================

auto framework_name(Framework f) -> std::string {
    switch (f) {
        case Framework::General: return "general";
        case Framework::SOC2: return "soc2";
        case Framework::ISO27001: return "iso27001";
    }
    return "general";
}

auto parse_framework(const std::string& s) -> std::optional<Framework> {
    if (s == "general") return Framework::General;
    if (s == "soc2") return Framework::SOC2;
    if (s == "iso27001") return Framework::ISO27001;
    return std::nullopt;
}

auto default_domains() -> std::vector<DomainDefinition> {
    return {
        {"Governance & Organizational Controls",
         {"contributing", "code review"}, {"CODEOWNERS", "governance docs"}, false},

        {"Legal & Regulatory Compliance",
         {"LICENSE"}, {"NOTICE", "privacy policy"}, false},

        {"Secure Software Development Lifecycle",
         {".gitignore", "CI", "SECURITY"}, {"branch protection"}, false},

        {"Identity & Access Management",
         {"secrets excluded"}, {"SSO", "2FA"}, false},

        {"Data Security & Privacy",
         {}, {"PII filter", "encryption"}, false},

        {"Infrastructure & Cloud Security",
         {}, {"Dockerfile", "cloud config"}, false},

        {"Application Security",
         {"security gate"}, {"dependency audit", "build flags"}, true},

        {"Vulnerability & Patch Management",
         {"lockfile"}, {"CVE scanning", "dependency versions"}, false},

        {"Logging, Monitoring & Detection",
         {}, {"audit log", "metrics"}, false},

        {"Incident Response",
         {}, {"security policy", "incident playbook"}, false},

        {"Business Continuity & Disaster Recovery",
         {}, {"backup", "recovery"}, false},

        {"Performance & Reliability",
         {}, {"SLA", "benchmark", "timeout"}, false},

        {"Software Quality Assurance",
         {"test gate", "build gate"}, {"coverage", "CI"}, true},

        {"Change Management",
         {"commit signing", "CHANGELOG"}, {"git history"}, false},

        {"Supply Chain & Third-Party Security",
         {"lockfile"}, {"SBOM", "dependency audit"}, false},

        {"Documentation & Audit Evidence",
         {"README"}, {"docs/", "architecture docs"}, true},

        {"User Protection & Safety",
         {}, {"PII filter", "input validation"}, false},

        {"Training & Awareness",
         {}, {"training", "onboarding", "security guidelines"}, false},
    };
}

void apply_framework_overlay(Framework f, std::vector<DomainDefinition>& domains) {
    if (f == Framework::General) {
        // General: domains 3(SDLC), 7(AppSec), 13(SQA), 16(Docs) are critical
        // (0-indexed: 2, 6, 12, 15)
        for (auto& d : domains) {
            if (d.name == "Secure Software Development Lifecycle" ||
                d.name == "Application Security" ||
                d.name == "Software Quality Assurance" ||
                d.name == "Documentation & Audit Evidence") {
                d.critical = true;
            }
        }
        return;
    }

    if (f == Framework::SOC2) {
        // SOC2: additionally 1(Governance), 4(IAM), 9(Logging), 14(Change Mgmt), 15(Supply Chain)
        for (auto& d : domains) {
            if (d.name == "Secure Software Development Lifecycle" ||
                d.name == "Application Security" ||
                d.name == "Software Quality Assurance" ||
                d.name == "Documentation & Audit Evidence" ||
                d.name == "Governance & Organizational Controls" ||
                d.name == "Identity & Access Management" ||
                d.name == "Logging, Monitoring & Detection" ||
                d.name == "Change Management" ||
                d.name == "Supply Chain & Third-Party Security") {
                d.critical = true;
            }
        }
        return;
    }

    if (f == Framework::ISO27001) {
        // ISO: additionally 1(Governance), 8(VulnMgmt), 10(Incident), 11(BC/DR), 15(SupplyChain)
        for (auto& d : domains) {
            if (d.name == "Secure Software Development Lifecycle" ||
                d.name == "Application Security" ||
                d.name == "Software Quality Assurance" ||
                d.name == "Documentation & Audit Evidence" ||
                d.name == "Governance & Organizational Controls" ||
                d.name == "Vulnerability & Patch Management" ||
                d.name == "Incident Response" ||
                d.name == "Business Continuity & Disaster Recovery" ||
                d.name == "Supply Chain & Third-Party Security") {
                d.critical = true;
            }
        }
    }
}

auto run_certification(Context& /*ctx*/,
                       const std::filesystem::path& target,
                       const RunOptions& opts) -> RunResult {
    RunResult result;
    result.framework = opts.framework;
    result.target = target.string();
    result.strict = opts.strict;

    // Known commands for docs accuracy check
    std::vector<std::string> known_commands = {
        "check", "triage", "review", "compare", "stats", "policy", "certify-readiness",
        "install", "update", "upgrade", "uninstall", "self",
        "doctor", "health", "verify", "agent", "playbook"
    };

    // --- Run gates ---

    // 1. Commit signing (blocking only in strict)
    result.gates.push_back(detail::gate_commit_signing(target, opts.commit_window, opts.since_ref, opts.strict));

    // 2. Unit test health
    if (opts.run_tests) {
        result.gates.push_back(detail::gate_unit_test_health(target));
    } else {
        result.gates.push_back({"unit_test_health", "skipped", "Skipped (--no-tests)", false, {{"status", "skipped"}}});
    }

    // 3. Build integrity
    if (opts.run_build) {
        result.gates.push_back(detail::gate_build_integrity(target));
    } else {
        result.gates.push_back({"build_integrity", "skipped", "Skipped (--no-build)", false, {{"status", "skipped"}}});
    }

    // 4. Documentation accuracy
    auto docs_gate = detail::gate_docs_accuracy(target, known_commands);
    if (opts.strict) docs_gate.blocking = true;
    result.gates.push_back(docs_gate);

    // 5. Security critical
    if (opts.run_security) {
        result.gates.push_back(detail::gate_security_critical(target));
    } else {
        result.gates.push_back({"security_critical", "skipped", "Skipped (--no-security)", false, {{"status", "skipped"}}});
    }

    // --- Quality risk analysis (not a gate) ---
    result.quality_risk = detail::analyze_quality_risk(target);

    // --- Collect evidence ---
    result.evidence = detail::collect_evidence(target, result.gates, result.quality_risk);

    // --- Evaluate domains ---
    auto domains = default_domains();
    apply_framework_overlay(opts.framework, domains);
    result.domains = detail::evaluate_domains(domains, result.evidence);

    // --- Finalize ---
    detail::finalize_result(result);

    return result;
}

auto RunResult::to_json() const -> nlohmann::json {
    nlohmann::json j;
    j["schema"] = "euxis.certification";
    j["schema_version"] = "1.0.0";
    j["assessment_type"] = "internal_readiness";
    j["official_certification"] = false;
    j["disclaimer"] = "This artifact is an internal readiness assessment and not proof of "
                      "external certification or legal compliance.";

    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time_t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    j["timestamp"] = buf;

    j["framework"] = framework_name(framework);
    j["target"] = target;
    j["strict"] = strict;
    j["status"] = status;
    j["confidence"] = confidence;
    j["readiness_summary"] = readiness_summary;

    // Gates
    j["gates"] = nlohmann::json::array();
    for (const auto& g : gates) {
        j["gates"].push_back({
            {"name", g.name}, {"status", g.status}, {"blocking", g.blocking},
            {"detail", g.detail}, {"data", g.data}
        });
    }

    // Domains
    j["domains"] = nlohmann::json::array();
    for (const auto& d : domains) {
        j["domains"].push_back({
            {"name", d.name}, {"status", d.status}, {"coverage", d.coverage},
            {"evidence_count", d.evidence_count}, {"critical", d.critical},
            {"findings", d.findings}, {"missing_controls", d.missing_controls}
        });
    }

    // Evidence summary
    int exec_backed = 0, inferred = 0;
    for (const auto& e : evidence) {
        if (e.execution_backed) ++exec_backed;
        else ++inferred;
    }
    j["evidence_summary"] = {
        {"total", static_cast<int>(evidence.size())},
        {"execution_backed", exec_backed},
        {"inferred", inferred}
    };

    // Quality risk
    j["quality_risk"] = quality_risk.data;

    // Gate-specific data sections
    for (const auto& g : gates) {
        j[g.name] = g.data;
    }

    j["blocking_issues"] = blocking_issues;
    j["major_gaps"] = major_gaps;
    j["recommended_actions"] = recommended_actions;
    j["skipped_gates"] = skipped_gates;
    j["exit_code"] = exit_code;

    return j;
}

} // namespace euxis::cli::certification
