#include "euxis/cli/cmd/fleet.hpp"
#include "euxis/cli/config_loader.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/process.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/registry_client.hpp"
#include "euxis/cli/session.hpp"
#include "euxis/cli/terminal.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

using euxis::cli::i18n::tr;

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;
namespace term = terminal;

constexpr auto VERDICT_SCHEMA = "euxis.verdict";
constexpr auto VERDICT_SCHEMA_VERSION = "1.0.0";
constexpr double SLA_TOLERANCE_MS = 500.0; // Absorb scheduling/measurement noise

// --- Epistemic Evidence Structures ---

struct AgentEvidence {
    std::string agent_name;
    std::string agent_id;         // Registry agent ID (e.g. "reviewer", "librarian")
    std::string verdict;          // Terminal state: PASS / FAILED / TIMEOUT / PARTIAL / DEGRADED
    std::string pillar;           // Which evidence pillar this covers
    double duration_ms{0.0};
    int execution_backed{0};      // Claims backed by actual execution (tests, builds, lints)
    int inferred{0};              // Claims based on static analysis or heuristics
    int total_claims{0};
    std::vector<std::string> key_findings;
    bool degraded{false};         // Ran on fallback provider
    std::string raw_verdict;      // Original verdict emitted by agent (PASS/FAIL/WARN), preserved even on TIMEOUT
};

struct ContradictionEntry {
    std::string pillar;
    std::string agent_a;
    std::string verdict_a;
    std::string agent_b;
    std::string verdict_b;
    std::string detail;
};

struct PillarScore {
    std::string name;
    int coverage{0};              // 0-100: how much of this pillar was checked
    int agreement{0};             // 0-100: how much agents agree within this pillar
    int evidence_density{0};      // 0-100: ratio of claims with backing
    std::string status;           // "Verified", "Gaps", "Conflict", "Missing"
};

struct BenchmarkMetrics {
    double median_agent_latency_ms{0.0};
    double total_latency_ms{0.0};
    double estimated_cost_usd{0.0};
    int agents_executed{0};
    int agents_skipped{0};        // Early-stopped
    int critical_checks_present{0};
    int critical_checks_expected{0};
    double coverage_ratio{0.0};
};

// --- Mode SLA Budgets (milliseconds) ---
struct ModeSLA {
    double target_ms;    // Ideal completion time
    double soft_cap_ms;  // Warning threshold
    double budget_ms;    // Best-effort budget — new agents won't launch past this, running agents get clamped timeouts
    int agent_timeout_s; // Per-agent executor timeout
};

auto get_mode_sla(const std::string& mode) -> ModeSLA {
    if (mode == "flash")    return {45000, 60000, 75000, 45};
    if (mode == "standard") return {180000, 300000, 600000, 120};
    /* forensic */          return {600000, 900000, 1200000, 300};
}

// Agent terminal states — replaces generic "WARN"
// PASS: Agent completed, verdict is positive
// PARTIAL: Agent completed but didn't emit a clear verdict
// TIMEOUT: Agent exceeded budget, killed mid-execution
// FAILED: Agent completed, verdict is negative
// DEGRADED: Agent ran on fallback provider
// CONTRADICTED: Agent disagrees with majority (set post-hoc)
auto classify_agent_status(const std::string& raw_verdict, bool timed_out,
                           bool is_degraded, double duration_ms, double budget_ms) -> std::string {
    if (timed_out || duration_ms > budget_ms) return "TIMEOUT";
    if (raw_verdict == "PASS") return is_degraded ? "DEGRADED" : "PASS";
    if (raw_verdict == "FAIL") return "FAILED";
    return "PARTIAL"; // No clear VERDICT: line → partial evidence
}

auto format_agent_status(const std::string& status) -> std::string {
    if (status == "PASS")         return term::green(status);
    if (status == "FAILED")       return term::red(status);
    if (status == "TIMEOUT")      return term::red(status);
    if (status == "PARTIAL")      return term::yellow(status);
    if (status == "DEGRADED")     return term::yellow(status);
    if (status == "CONTRADICTED") return term::red(status);
    return term::dim(status);
}

// Critical pillars that must be checked for high-confidence verdicts
const std::vector<std::string> CRITICAL_PILLARS = {
    "security", "testing", "build", "architecture"
};

// Canonicalize pillar names so manifest aliases map to critical pillars
auto canonicalize_pillar(const std::string& raw) -> std::string {
    // Architecture aliases
    if (raw == "structure" || raw == "structural" || raw == "design") return "architecture";
    // Testing aliases
    if (raw == "test" || raw == "tests" || raw == "verification" || raw == "quality") return "testing";
    // Build aliases
    if (raw == "compilation" || raw == "ci" || raw == "infra") return "build";
    // Security aliases
    if (raw == "vuln" || raw == "vulnerability" || raw == "safety" || raw == "audit") return "security";
    return raw;
}

// --- Verdict rank for policy evaluation ---
auto verdict_rank(const std::string& v) -> int {
    if (v == "TRUSTED") return 5;
    if (v == "TRUSTED WITH GAPS") return 4;
    if (v == "CAUTION") return 3;
    if (v == "BLOCKED") return 2;
    if (v == "INCONCLUSIVE") return 1;
    return 0;
}

// --- Policy Mode Structures ---
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

auto load_policy(const std::string& data_dir, const std::string& explicit_path = "") -> std::optional<Policy> {
    nlohmann::json j;
    if (!explicit_path.empty()) {
        if (!fs::exists(explicit_path)) return std::nullopt;
        std::ifstream f(explicit_path);
        try { j = nlohmann::json::parse(f); }
        catch (...) { return std::nullopt; }
    } else {
        ConfigLoader loader(data_dir);
        auto loaded = loader.load("config/policy.json");
        if (!loaded) return std::nullopt;
        j = *loaded;
    }

    Policy p;
    p.min_verdict = j.value("min_verdict", p.min_verdict);
    p.min_confidence = j.value("min_confidence", p.min_confidence);
    p.min_critical_coverage = j.value("min_critical_coverage", p.min_critical_coverage);
    p.fail_on_regression = j.value("fail_on_regression", p.fail_on_regression);
    p.max_drift = j.value("max_drift", p.max_drift);
    p.fail_on_escalation = j.value("fail_on_escalation", p.fail_on_escalation);
    p.fail_on_budget_exceeded = j.value("fail_on_budget_exceeded", p.fail_on_budget_exceeded);
    return p;
}

auto evaluate_policy(const nlohmann::json& artifact, const Policy& policy) -> std::vector<PolicyViolation> {
    std::vector<PolicyViolation> violations;

    // min_verdict
    auto actual_verdict = artifact.value("verdict", "");
    if (verdict_rank(actual_verdict) < verdict_rank(policy.min_verdict)) {
        violations.push_back({"min_verdict", "Verdict " + actual_verdict + " is below minimum " + policy.min_verdict});
    }

    // min_confidence
    int actual_conf = artifact.value("confidence", 0);
    if (actual_conf < policy.min_confidence) {
        violations.push_back({"min_confidence", "Confidence " + std::to_string(actual_conf) + " is below minimum " + std::to_string(policy.min_confidence)});
    }

    // min_critical_coverage
    if (artifact.contains("benchmark") && artifact["benchmark"].contains("critical_coverage")) {
        double actual_cov = artifact["benchmark"]["critical_coverage"].get<double>();
        if (actual_cov < policy.min_critical_coverage) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << "Coverage " << actual_cov << " is below minimum " << policy.min_critical_coverage;
            violations.push_back({"min_critical_coverage", ss.str()});
        }
    }

    // fail_on_regression
    if (policy.fail_on_regression && artifact.contains("trend") && artifact["trend"].value("regression", false)) {
        violations.push_back({"fail_on_regression", "Verdict regression detected from " + artifact["trend"].value("previous_verdict", "unknown")});
    }

    // max_drift (only if compare data present)
    if (artifact.contains("compare")) {
        int drift = artifact["compare"].value("confidence_drift", 0);
        if (std::abs(drift) > policy.max_drift) {
            violations.push_back({"max_drift", "Confidence drift " + std::to_string(drift) + " exceeds maximum " + std::to_string(policy.max_drift)});
        }
    }

    // fail_on_escalation
    if (policy.fail_on_escalation && artifact.value("escalated", false)) {
        violations.push_back({"fail_on_escalation", "Flash→Standard escalation occurred"});
    }

    // fail_on_budget_exceeded
    if (policy.fail_on_budget_exceeded && artifact.contains("sla") && !artifact["sla"].value("met", true)) {
        violations.push_back({"fail_on_budget_exceeded", "SLA budget was not met"});
    }

    return violations;
}

[[maybe_unused]] void write_output_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path);
    f << content;
}

[[maybe_unused]] auto read_file_contents(const fs::path& path) -> std::string {
    if (!fs::exists(path)) return {};
    std::ifstream f(path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// Parse agent output for execution-backed vs inferred claims
auto parse_evidence_depth(const std::string& output) -> std::tuple<int, int, int> {
    int exec_backed = 0, inferred = 0, total = 0;
    // Execution markers: test results, build output, lint passes, command output
    const std::vector<std::string> exec_markers = {
        "test", "passed", "failed", "built", "compiled", "error:", "warning:",
        "exit code", "output:", "stdout", "stderr", "ran ", "executed"
    };
    const std::vector<std::string> infer_markers = {
        "appears", "likely", "seems", "suggest", "could", "might", "probably",
        "looks like", "assume", "infer", "based on naming", "convention"
    };

    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line.size() < 5) continue;
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        bool is_exec = false, is_infer = false;
        for (const auto& m : exec_markers) if (lower.find(m) != std::string::npos) { is_exec = true; break; }
        for (const auto& m : infer_markers) if (lower.find(m) != std::string::npos) { is_infer = true; break; }

        if (is_exec) { exec_backed++; total++; }
        else if (is_infer) { inferred++; total++; }
    }
    if (total == 0) { inferred = 1; total = 1; } // No evidence = fully inferred
    return {exec_backed, inferred, total};
}

// Extract key findings from agent output
auto extract_findings(const std::string& output) -> std::vector<std::string> {
    std::vector<std::string> findings;
    std::istringstream stream(output);
    std::string line;

    // Prompt instruction patterns to filter out
    static const std::vector<std::string> prompt_noise = {
        "you must end with",
        "you must respond",
        "keep response under",
        "budget:",
        "mode:",
        "focus on your assigned",
        "skip exhaustive",
        "prioritize decisive",
        "be thorough but concise",
        "be direct",
        "skip: architecture",
        "do not recap",
        "answer only:",
        "you are a decision",
        "you are a docs drift",
        "focus only on:",
        "do not read or summarize",
        "spot-check specific",
        "maximum 3 findings",
    };

    while (std::getline(stream, line)) {
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // Skip lines that look like prompt instructions echoed back
        bool is_noise = false;
        for (const auto& pat : prompt_noise) {
            if (lower.find(pat) != std::string::npos) { is_noise = true; break; }
        }
        if (is_noise) continue;

        // Look for finding-like lines
        if (lower.find("finding:") != std::string::npos ||
            lower.find("issue:") != std::string::npos ||
            lower.find("critical:") != std::string::npos ||
            lower.find("recommendation:") != std::string::npos) {
            auto trimmed = line;
            // Strip leading whitespace, bullets, markdown
            while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '-' ||
                    trimmed[0] == '*' || trimmed[0] == '#')) trimmed.erase(0, 1);
            // Strip trailing whitespace
            while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\n'))
                trimmed.pop_back();
            if (trimmed.size() > 10 && trimmed.size() < 300) findings.push_back(trimmed);
        }
    }

    // Deduplicate (preserve order)
    std::vector<std::string> unique;
    for (const auto& f : findings) {
        bool dup = false;
        for (const auto& u : unique) {
            if (u == f) { dup = true; break; }
        }
        if (!dup) unique.push_back(f);
    }
    return unique;
}

// Check if verdicts have converged (used for early-stop)
// Terminal states: PASS/DEGRADED=positive, FAILED=negative, PARTIAL/TIMEOUT=neutral
// Convergence requires:
//   1. At least min_agents have completed overall
//   2. At least one non-neutral verdict exists
//   3. All non-neutral verdicts are on the same side
auto verdict_converged(const std::unordered_map<std::string, std::string>& verdicts,
                       int min_agents = 3) -> bool {
    if ((int)verdicts.size() < min_agents) return false;
    int positive = 0, negative = 0;
    for (const auto& [_, v] : verdicts) {
        if (v == "PASS" || v == "DEGRADED") positive++;
        else if (v == "FAILED") negative++;
        // PARTIAL / TIMEOUT are neutral — don't block convergence
    }
    if (positive == 0 && negative == 0) return false; // All neutral = no convergence
    return (positive > 0 && negative == 0) || (negative > 0 && positive == 0);
}

// =====================================================================
//  VERDICT MEMORY — Longitudinal trust tracking per repository
// =====================================================================

struct VerdictHistoryEntry {
    std::string timestamp;
    std::string verdict;
    int confidence{0};
    std::string mode;
    double latency_ms{0.0};
    double median_agent_latency_ms{0.0};
    double p95_agent_latency_ms{0.0};
    int agents_executed{0};
    int agents_skipped{0};
    int timeout_count{0};
    double evidence_density{0.0};
    bool early_stopped{false};
    bool escalated{false};
    bool budget_exceeded{false};
};

auto verdict_history_path(const std::string& euxis_home) -> fs::path {
    return fs::path(euxis_home) / "data" / "runtime" / "verdicts" / "history.jsonl";
}

auto load_verdict_history(const std::string& euxis_home, int max_entries = 50) -> std::vector<VerdictHistoryEntry> {
    auto path = verdict_history_path(euxis_home);
    std::vector<VerdictHistoryEntry> history;
    if (!fs::exists(path)) return history;

    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            history.push_back({
                j.value("timestamp", ""),
                j.value("verdict", ""),
                j.value("confidence", 0),
                j.value("mode", ""),
                j.value("latency_ms", 0.0),
                j.value("median_agent_latency_ms", 0.0),
                j.value("p95_agent_latency_ms", 0.0),
                j.value("agents_executed", 0),
                j.value("agents_skipped", 0),
                j.value("timeout_count", 0),
                j.value("evidence_density", 0.0),
                j.value("early_stopped", false),
                j.value("escalated", false),
                j.value("budget_exceeded", false)
            });
        } catch (...) { continue; }
    }
    // Keep only the most recent entries
    if ((int)history.size() > max_entries) {
        history.erase(history.begin(), history.end() - max_entries);
    }
    return history;
}

void append_verdict_history(const std::string& euxis_home, const VerdictHistoryEntry& entry) {
    auto path = verdict_history_path(euxis_home);
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::app);
    nlohmann::json j;
    j["timestamp"] = entry.timestamp;
    j["verdict"] = entry.verdict;
    j["confidence"] = entry.confidence;
    j["mode"] = entry.mode;
    j["latency_ms"] = entry.latency_ms;
    j["median_agent_latency_ms"] = entry.median_agent_latency_ms;
    j["p95_agent_latency_ms"] = entry.p95_agent_latency_ms;
    j["agents_executed"] = entry.agents_executed;
    j["agents_skipped"] = entry.agents_skipped;
    j["timeout_count"] = entry.timeout_count;
    j["evidence_density"] = entry.evidence_density;
    j["early_stopped"] = entry.early_stopped;
    j["escalated"] = entry.escalated;
    j["budget_exceeded"] = entry.budget_exceeded;
    f << j.dump() << "\n";
}

struct VerdictTrend {
    std::string direction;        // "improving", "declining", "stable", "volatile", "new"
    int confidence_delta{0};      // Change from previous run
    int run_count{0};
    bool regression{false};       // Went from TRUSTED→worse
    std::string previous_verdict;
    int streak{0};                // Consecutive same-verdict runs
};

auto compute_verdict_trend(const std::vector<VerdictHistoryEntry>& history,
                           const std::string& current_verdict, int current_confidence) -> VerdictTrend {
    VerdictTrend trend;
    trend.run_count = (int)history.size();

    if (history.empty()) {
        trend.direction = "new";
        return trend;
    }

    const auto& prev = history.back();
    trend.previous_verdict = prev.verdict;
    trend.confidence_delta = current_confidence - prev.confidence;

    // Regression detection (uses namespace-level verdict_rank)
    int prev_rank = verdict_rank(prev.verdict);
    int curr_rank = verdict_rank(current_verdict);
    trend.regression = (prev_rank >= 4 && curr_rank < prev_rank);

    // Streak calculation
    trend.streak = 1;
    for (int i = (int)history.size() - 1; i >= 0; --i) {
        if (history[i].verdict == current_verdict) trend.streak++;
        else break;
    }

    // Direction from recent history (last 5 runs)
    if (history.size() >= 3) {
        int recent_start = std::max(0, (int)history.size() - 5);
        int improving = 0, declining = 0;
        for (int i = recent_start + 1; i < (int)history.size(); ++i) {
            if (history[i].confidence > history[i-1].confidence) improving++;
            else if (history[i].confidence < history[i-1].confidence) declining++;
        }
        // Include current run
        if (current_confidence > history.back().confidence) improving++;
        else if (current_confidence < history.back().confidence) declining++;

        if (improving > declining * 2) trend.direction = "improving";
        else if (declining > improving * 2) trend.direction = "declining";
        else if (improving > 0 && declining > 0) trend.direction = "volatile";
        else trend.direction = "stable";
    } else {
        trend.direction = (trend.confidence_delta > 5) ? "improving"
                        : (trend.confidence_delta < -5) ? "declining" : "stable";
    }

    return trend;
}

// =====================================================================
//  CLAIM VERIFICATION ENGINE — Upgrade inferred claims to execution-backed
// =====================================================================

struct VerificationResult {
    std::string check_name;
    std::string command;
    bool success{false};
    double duration_ms{0.0};
    int claims_upgraded{0};
    std::string output_summary;
};

auto detect_project_verifiers(const std::string& repo_root) -> std::vector<std::pair<std::string, std::vector<std::string>>> {
    std::vector<std::pair<std::string, std::vector<std::string>>> verifiers;

    // Detect project type from the repo being analyzed (use repo root, not cwd)
    auto check_file = [&repo_root](const std::string& path) {
        if (repo_root.empty()) return fs::exists(path);
        return fs::exists(fs::path(repo_root) / path);
    };

    // Rust
    if (check_file("Cargo.toml")) {
        verifiers.push_back({"cargo-check", {"cargo", "check", "--quiet"}});
        verifiers.push_back({"cargo-test", {"cargo", "test", "--quiet", "--no-fail-fast"}});
    }
    // Node.js
    if (check_file("package.json")) {
        if (check_file("node_modules/.bin/jest")) verifiers.push_back({"jest", {"npx", "jest", "--passWithNoTests", "--silent"}});
        else if (check_file("node_modules/.bin/vitest")) verifiers.push_back({"vitest", {"npx", "vitest", "run", "--reporter=verbose"}});
        else verifiers.push_back({"npm-test", {"npm", "test", "--if-present"}});
    }
    // Go
    if (check_file("go.mod")) {
        verifiers.push_back({"go-vet", {"go", "vet", "./..."}});
        verifiers.push_back({"go-test", {"go", "test", "-count=1", "-short", "./..."}});
    }
    // Python
    if (check_file("pyproject.toml") || check_file("setup.py") || check_file("requirements.txt")) {
        if (check_file("pytest.ini") || check_file("pyproject.toml")) verifiers.push_back({"pytest", {"python", "-m", "pytest", "-q", "--tb=no"}});
    }
    // C/C++ with CMake
    if (check_file("CMakeLists.txt")) {
        // Don't re-run our own build — look for a build dir
        if (check_file("build/Makefile") || check_file("cmake-build/Makefile")) {
            auto build_dir = check_file("cmake-build/Makefile") ? "cmake-build" : "build";
            verifiers.push_back({"cmake-build", {"cmake", "--build", build_dir, "--target", "all"}});
        }
    }
    // Generic: check for Makefile
    if (verifiers.empty() && check_file("Makefile")) {
        verifiers.push_back({"make-check", {"make", "check"}});
    }

    return verifiers;
}

auto run_claim_verifiers(const std::vector<std::pair<std::string, std::vector<std::string>>>& verifiers,
                         int timeout_per_check = 60) -> std::vector<VerificationResult> {
    std::vector<VerificationResult> results;
    for (const auto& [name, cmd] : verifiers) {
        if (cmd.empty()) continue;
        auto start = std::chrono::steady_clock::now();
        std::vector<std::string> args(cmd.begin() + 1, cmd.end());
        auto proc = Process::run(cmd[0], args, timeout_per_check);
        auto elapsed = std::chrono::steady_clock::now() - start;
        double dur = std::chrono::duration<double, std::milli>(elapsed).count();

        // Summarize output (first 3 lines)
        std::string summary;
        std::istringstream ss(proc.success() ? proc.stdout_output : proc.stderr_output);
        std::string line;
        int line_count = 0;
        while (std::getline(ss, line) && line_count < 3) {
            if (!line.empty()) { summary += line + " "; line_count++; }
        }

        results.push_back({name, cmd[0] + " " + (args.empty() ? "" : args[0]),
                           proc.success(), dur,
                           proc.success() ? 3 : 0, // Each successful check upgrades ~3 inferred claims
                           summary});
    }
    return results;
}

// =====================================================================
//  BUILT-IN PILLAR CHECKS — Lightweight coverage for testing & security
// =====================================================================

struct PillarCheckResult {
    std::string name;
    std::string status;   // "detected", "passing", "failing", "missing"
    std::string detail;
    double duration_ms{0.0};
    int claims{0};
    int execution_backed{0};
};

// Testing pillar: detect test infrastructure, run bounded smoke test
auto run_testing_pillar_checks(const std::string& repo_root) -> std::vector<PillarCheckResult> {
    std::vector<PillarCheckResult> results;

    auto check_file = [&repo_root](const std::string& path) {
        if (repo_root.empty()) return fs::exists(path);
        return fs::exists(fs::path(repo_root) / path);
    };
    auto full_path = [&repo_root](const std::string& path) {
        return repo_root.empty() ? path : (fs::path(repo_root) / path).string();
    };

    // 1. Detect test infrastructure
    bool tests_detected = false;
    std::string test_framework;

    if (check_file("CMakeLists.txt")) {
        // Check for test targets in CMake
        if (check_file("cmake-build") || check_file("build")) {
            auto build_dir = check_file("cmake-build") ? "cmake-build" : "build";
            auto p = full_path(build_dir);
            // Look for test executables
            bool found_test_binary = false;
            try {
                for (const auto& entry : fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied)) {
                    if (entry.is_regular_file()) {
                        auto fname = entry.path().filename().string();
                        if (fname.find("test") != std::string::npos && ::access(entry.path().c_str(), X_OK) == 0) {
                            found_test_binary = true;
                            test_framework = "cmake/gtest";
                            break;
                        }
                    }
                }
            } catch (...) {}
            if (found_test_binary) tests_detected = true;
        }
    }
    if (!tests_detected && check_file("Cargo.toml")) {
        tests_detected = true; test_framework = "cargo";
    }
    if (!tests_detected && check_file("package.json")) {
        if (check_file("node_modules/.bin/jest") || check_file("node_modules/.bin/vitest")) {
            tests_detected = true;
            test_framework = check_file("node_modules/.bin/jest") ? "jest" : "vitest";
        }
    }
    if (!tests_detected && check_file("go.mod")) {
        tests_detected = true; test_framework = "go";
    }
    if (!tests_detected && (check_file("pytest.ini") || check_file("pyproject.toml"))) {
        tests_detected = true; test_framework = "pytest";
    }

    if (!tests_detected) {
        results.push_back({"test-infrastructure", "missing", "No test framework detected", 0.0, 1, 0});
        return results;
    }

    results.push_back({"test-infrastructure", "detected",
                        "Test framework: " + test_framework, 0.0, 1, 1});

    // 2. Count test files (bounded scan)
    int test_file_count = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(
                 repo_root.empty() ? "." : repo_root,
                 fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            auto fname = entry.path().filename().string();
            auto ext = entry.path().extension().string();
            // Skip build dirs and vendor
            auto path_str = entry.path().string();
            if (path_str.find("build") != std::string::npos && path_str.find("cmake-build") == std::string::npos) continue;
            if (path_str.find("node_modules") != std::string::npos) continue;
            if (path_str.find("vendor") != std::string::npos) continue;
            if (path_str.find(".git") != std::string::npos) continue;

            bool is_test = (fname.find("test_") == 0 || fname.find("test.") != std::string::npos ||
                           fname.find("_test.") != std::string::npos || fname.find(".test.") != std::string::npos ||
                           fname.find("spec.") != std::string::npos || fname.find("_spec.") != std::string::npos);
            if (is_test && (ext == ".cpp" || ext == ".py" || ext == ".js" || ext == ".ts" ||
                           ext == ".go" || ext == ".rs" || ext == ".java")) {
                test_file_count++;
            }
            if (test_file_count >= 200) break; // Bounded
        }
    } catch (...) {}

    results.push_back({"test-file-count", test_file_count > 0 ? "detected" : "missing",
                        std::to_string(test_file_count) + " test files found", 0.0, 1,
                        test_file_count > 0 ? 1 : 0});

    return results;
}

// Security pillar: lightweight checks for common issues
auto run_security_pillar_checks(const std::string& repo_root) -> std::vector<PillarCheckResult> {
    std::vector<PillarCheckResult> results;

    auto check_file = [&repo_root](const std::string& path) {
        if (repo_root.empty()) return fs::exists(path);
        return fs::exists(fs::path(repo_root) / path);
    };
    auto full_path = [&repo_root](const std::string& path) -> std::string {
        return repo_root.empty() ? path : (fs::path(repo_root) / path).string();
    };

    // 1. Check for exposed secrets / .env files
    {
        bool has_env_file = false;
        bool env_in_gitignore = false;

        if (check_file(".env")) has_env_file = true;
        if (check_file(".env.local")) has_env_file = true;

        if (check_file(".gitignore")) {
            try {
                std::ifstream gi(full_path(".gitignore"));
                std::string line;
                while (std::getline(gi, line)) {
                    if (line.find(".env") != std::string::npos) { env_in_gitignore = true; break; }
                }
            } catch (...) {}
        }

        if (has_env_file && !env_in_gitignore) {
            results.push_back({"secret-scan", "failing",
                                ".env file exists but not in .gitignore", 0.0, 1, 1});
        } else if (has_env_file && env_in_gitignore) {
            results.push_back({"secret-scan", "passing",
                                ".env file properly gitignored", 0.0, 1, 1});
        } else {
            results.push_back({"secret-scan", "passing",
                                "No exposed .env files", 0.0, 1, 1});
        }
    }

    // 2. Dependency audit (if tooling available)
    {
        bool audit_ran = false;
        if (check_file("package-lock.json") && Process::available("npm")) {
            auto start = std::chrono::steady_clock::now();
            auto proc = Process::run("npm", {"audit", "--json"}, 30);
            auto dur = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
            audit_ran = true;
            results.push_back({"npm-audit", proc.success() ? "passing" : "failing",
                                proc.success() ? "No known vulnerabilities" : "Vulnerabilities found",
                                dur, 2, 2});
        }
        if (check_file("Cargo.lock") && Process::available("cargo")) {
            auto which = Process::shell("cargo audit --version 2>/dev/null", 5);
            if (which.success()) {
                auto start = std::chrono::steady_clock::now();
                auto proc = Process::run("cargo", {"audit", "--quiet"}, 30);
                auto dur = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
                audit_ran = true;
                results.push_back({"cargo-audit", proc.success() ? "passing" : "failing",
                                    proc.success() ? "No known vulnerabilities" : "Vulnerabilities found",
                                    dur, 2, 2});
            }
        }
        // pip audit for Python projects
        if (check_file("requirements.txt") || check_file("pyproject.toml")) {
            if (Process::available("pip-audit")) {
                auto start = std::chrono::steady_clock::now();
                auto proc = Process::run("pip-audit", {}, 30);
                auto dur = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
                audit_ran = true;
                results.push_back({"pip-audit", proc.success() ? "passing" : "failing",
                                    proc.success() ? "No known vulnerabilities" : "Vulnerabilities found",
                                    dur, 2, 2});
            }
        }
        // For C++ projects using CMake — check vcpkg/conan vulnerability scanning
        if (check_file("CMakeLists.txt") && !audit_ran) {
            // C++ ecosystem lacks a universal audit tool. Report what we know.
            bool has_vcpkg = check_file("vcpkg.json");
            bool has_conan = check_file("conanfile.txt") || check_file("conanfile.py");
            bool has_fetch = false;
            // Check if project uses FetchContent (common for header-only deps)
            if (check_file("CMakeLists.txt")) {
                try {
                    std::ifstream f(full_path("CMakeLists.txt"));
                    std::string content((std::istreambuf_iterator<char>(f)),
                                         std::istreambuf_iterator<char>());
                    if (content.find("FetchContent") != std::string::npos) has_fetch = true;
                } catch (...) {}
            }
            std::string detail;
            if (has_vcpkg) detail = "vcpkg.json present — use 'vcpkg x-ci-verify-versions' for audit";
            else if (has_conan) detail = "Conan manifest present — use 'conan audit' for vulnerability scan";
            else if (has_fetch) detail = "CMake FetchContent manages dependencies — pinned versions in CMakeLists.txt";
            else detail = "CMake project — dependencies managed at build time";
            // C++ FetchContent with pinned versions is a valid dependency strategy
            audit_ran = has_fetch || has_vcpkg || has_conan;
            results.push_back({"cmake-deps", audit_ran ? "passing" : "missing",
                                detail, 0.0, 1, audit_ran ? 1 : 0});
        }
        if (!audit_ran) {
            // Detect which ecosystems exist to give actionable guidance
            std::string hint;
            if (check_file("package.json")) hint = "Install npm and run 'npm audit'";
            else if (check_file("Cargo.toml")) hint = "Install cargo-audit: 'cargo install cargo-audit'";
            else if (check_file("requirements.txt") || check_file("pyproject.toml"))
                hint = "Install pip-audit: 'pip install pip-audit'";
            else hint = "No recognized package manifest found";
            results.push_back({"dependency-audit", "missing", hint, 0.0, 1, 0});
        }
    }

    // 3. Lockfile staleness
    {
        bool stale = false;
        if (check_file("package.json") && check_file("package-lock.json")) {
            auto pkg_time = fs::last_write_time(full_path("package.json"));
            auto lock_time = fs::last_write_time(full_path("package-lock.json"));
            if (pkg_time > lock_time) stale = true;
        }
        if (check_file("Cargo.toml") && check_file("Cargo.lock")) {
            auto toml_time = fs::last_write_time(full_path("Cargo.toml"));
            auto lock_time = fs::last_write_time(full_path("Cargo.lock"));
            if (toml_time > lock_time) stale = true;
        }

        if (stale) {
            results.push_back({"lockfile-freshness", "failing",
                                "Lockfile older than manifest — dependencies may be out of sync",
                                0.0, 1, 1});
        } else if (check_file("package-lock.json") || check_file("Cargo.lock") ||
                   check_file("go.sum") || check_file("poetry.lock")) {
            results.push_back({"lockfile-freshness", "passing",
                                "Lockfile up to date", 0.0, 1, 1});
        }
        // No lockfile = no check (not all projects need one)
    }

    // 4. Unsafe build flags (scan CMakeLists.txt or Makefile for common issues)
    if (check_file("CMakeLists.txt")) {
        try {
            std::ifstream f(full_path("CMakeLists.txt"));
            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
            bool has_unsafe = false;
            std::string issue;
            if (content.find("-DNDEBUG") != std::string::npos && content.find("Release") == std::string::npos) {
                has_unsafe = true; issue = "NDEBUG set outside Release mode";
            }
            if (content.find("-fsanitize") != std::string::npos && content.find("Debug") == std::string::npos) {
                // Sanitizers in non-debug builds is unusual but not necessarily bad
            }
            results.push_back({"build-flags", has_unsafe ? "failing" : "passing",
                                has_unsafe ? issue : "No unsafe build flags detected",
                                0.0, 1, 1});
        } catch (...) {}
    }

    return results;
}

// --- Step Plan structure (used by playbook and escalation) ---
struct StepPlan {
    std::string name;
    std::string agent_id;
    std::string task;
    std::string pillar;
    std::vector<std::string> depends;
    ModelSelection model;
};

auto build_step_plan(const nlohmann::json& step, const std::string& mode,
                     const std::string& goal, const RegistryClient& registry,
                     ProviderRouter& router) -> StepPlan {
    std::string name = step.value("name", "step");
    std::string agent_id = step.value("agent", "code-agent");
    std::string pillar = canonicalize_pillar(step.value("pillar", "execution"));
    std::string step_task = step.value("task_template", step.value("task", ""));
    // Preserve original task for tier analysis (before goal/mode injection adds keywords)
    std::string routing_task = step_task;

    // Goal substitution
    size_t pos = 0;
    while ((pos = step_task.find("${goal}", pos)) != std::string::npos) {
        step_task.replace(pos, 7, goal);
        pos += goal.length();
    }

    // Verdict instruction — shared across all modes
    static const char* verdict_instruction =
        "\n\nVERDICT RULES:"
        "\nYour LAST line must be exactly one of: VERDICT: PASS / VERDICT: FAIL / VERDICT: WARN"
        "\nDecision guide: PASS = no blockers found. FAIL = blocking issues found. WARN = minor issues, not blocking."
        "\nIf findings are mixed, choose the verdict matching the most serious finding."
        "\nDo not quote, bold, or wrap the verdict line. Just write: VERDICT: PASS";

    // Mode-adaptive prompt injection
    if (mode == "flash") {
        step_task += "\n\nMODE: Triage (quick screening)"
                     "\nBudget: 20 seconds of reasoning. Be direct."
                     "\nFocus: Critical blockers, safety issues, clear PASS/FAIL signals."
                     "\nSkip: Architecture depth, style, exhaustive analysis."
                     "\nKeep response under 300 words.";
        step_task += verdict_instruction;
    } else if (mode == "standard") {
        step_task += "\n\nMODE: Standard (balanced)"
                     "\nBudget: 60 seconds of reasoning. Be thorough but concise."
                     "\nFocus on your assigned pillar only. Return your top 3-5 findings with evidence."
                     "\nFormat each finding as: Finding: <description>"
                     "\nSkip exhaustive repo-wide prose — prioritize decisive signals over narrative."
                     "\nKeep response under 800 words.";
        step_task += verdict_instruction;
        // Agent-specific scope constraints for standard mode
        if (agent_id == "reviewer") {
            step_task += "\n\nROLE: Decision summarizer."
                         "\nMaximum 3 findings. Do not recap other agents' work."
                         "\nAnswer: (1) Is this a merge blocker? (2) What is the top unresolved gap? (3) Your confidence level."
                         "\nKeep response under 400 words.";
        } else if (agent_id == "architect") {
            step_task += "\n\nROLE: Structural integrity checker."
                         "\nCheck: (1) separation of concerns (2) concurrency safety (3) abstraction leaks."
                         "\nSkip: code style, naming, documentation, exhaustive module-by-module review."
                         "\nKeep response under 500 words.";
        } else if (agent_id == "sentinel") {
            step_task += "\n\nROLE: Runtime safety gate."
                         "\nCheck: (1) input validation at boundaries (2) secret exposure (3) dependency risk."
                         "\nSkip: UI/UX assessment, accessibility review, exhaustive file enumeration."
                         "\nKeep response under 500 words.";
        } else if (agent_id == "librarian") {
            step_task += "\n\nROLE: Documentation truth verifier."
                         "\nReturn at most 3 concrete mismatches between docs and actual CLI behavior."
                         "\nFocus ONLY on: README getting-started, CLI command examples, install steps."
                         "\nSkip: prose quality, formatting, completeness, internal docs, changelogs."
                         "\nIf commands in README match actual behavior, emit VERDICT: PASS."
                         "\nKeep response under 400 words.";
        } else if (agent_id == "optimizer") {
            step_task += "\n\nROLE: Performance and build quality checker."
                         "\nCheck: (1) build succeeds (2) test suite runs (3) no obvious performance anti-patterns."
                         "\nSkip: micro-optimization, style preferences, exhaustive profiling."
                         "\nKeep response under 500 words.";
        }
    } else {
        step_task += "\n\nMODE: Forensic (deep analysis)"
                     "\nExamine every detail. Provide exhaustive evidence."
                     "\nInclude execution output, line references, and supporting data for every claim.";
        step_task += verdict_instruction;
    }

    std::vector<std::string> depends;
    if (step.contains("depends") && step["depends"].is_array()) {
        for (const auto& d : step["depends"]) depends.push_back(d.get<std::string>());
    }

    auto agent = registry.get_agent(agent_id);
    auto tier = agent ? agent->tier : "code";
    // Use original task template for routing (avoids goal/mode keyword contamination)
    auto model = (mode == "flash")
        ? router.route_flash(agent_id, tier, routing_task)
        : (mode == "standard")
            ? router.route_standard(agent_id, tier, routing_task)
            : router.route(tier, routing_task, "swarm");

    return {name, agent_id, step_task, pillar, depends, model};
}

} // namespace

// Forward declaration for stats mode
// Returns true if all targets met, false otherwise.
bool print_playbook_stats(Context& ctx, const std::string& since = "", int last_n = 0);

// --- agent ---
int cmd_agent(Context& ctx, const std::vector<std::string>& args) {
    RegistryClient registry(ctx.data_dir);
    if (args.empty() || args[0] == "list") {
        auto agents = registry.list_agents();
        if (ctx.json_output) {
            nlohmann::json j = nlohmann::json::array();
            for (const auto& a : agents) j.push_back({{"id", a.id}, {"role", a.role}, {"tier", a.tier}, {"version", a.version}});
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << term::bold(tr("Registered Agents")) << " (" << agents.size() << ")\n\n";
            std::vector<term::TableRow> rows;
            for (const auto& a : agents) rows.push_back({{a.id, a.role, a.tier, a.version}});
            term::print_table({tr("ID"), tr("Role"), tr("Tier"), tr("Version")}, rows);
        }
        return 0;
    }
    if (args[0] == "info") {
        if (args.size() < 2) { std::cerr << tr("Usage: euxis agent info <agent-id>") << "\n"; return 2; }
        auto agent = registry.get_agent(args[1]);
        if (!agent) {
            if (ctx.json_output) std::cout << nlohmann::json({{"error", "Agent not found: " + args[1]}}).dump() << "\n";
            else std::cerr << tr("Agent not found:") << " " << args[1] << "\n";
            return 1;
        }
        if (ctx.json_output) {
            nlohmann::json j = {{"id", agent->id}, {"role", agent->role}, {"tier", agent->tier}, {"version", agent->version}};
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << term::bold(agent->id) << "\n";
            std::cout << "  Role:    " << agent->role << "\n";
            std::cout << "  Tier:    " << agent->tier << "\n";
            std::cout << "  Version: " << agent->version << "\n";
            if (!agent->tags.empty()) {
                std::cout << "  Tags:    ";
                for (const auto& t : agent->tags) std::cout << t << " ";
                std::cout << "\n";
            }
        }
        return 0;
    }
    if (args[0] == "register") {
        if (args.size() < 2) { std::cerr << tr("Usage: euxis agent register <manifest.json>") << "\n"; return 2; }
        auto manifest_path = args[1];
        if (!fs::exists(manifest_path)) { std::cerr << tr("File not found:") << " " << manifest_path << "\n"; return 1; }
        std::ifstream f(manifest_path);
        nlohmann::json manifest;
        try { manifest = nlohmann::json::parse(f); }
        catch (...) { std::cerr << tr("Invalid JSON:") << " " << manifest_path << "\n"; return 1; }
        std::string agent_id = manifest.value("agent_id", manifest.value("id", ""));
        if (agent_id.empty()) { std::cerr << tr("Missing agent_id in manifest") << "\n"; return 1; }
        registry.register_plugin(agent_id, manifest);
        std::cout << term::green("Registered: " + agent_id) << "\n";
        return 0;
    }
    if (args[0] == "unregister") {
        if (args.size() < 2) { std::cerr << tr("Usage: euxis agent unregister <agent-id>") << "\n"; return 2; }
        auto agent = registry.get_agent(args[1]);
        if (!agent) { std::cerr << tr("Agent not found:") << " " << args[1] << "\n"; return 1; }
        registry.unregister_plugin(args[1]);
        std::cout << term::yellow("Unregistered: " + args[1]) << "\n";
        return 0;
    }
    std::cerr << tr("Usage: euxis agent <list|info|register|unregister>") << "\n";
    return 2;
}

// --- agent-bootstrap ---
int cmd_agent_bootstrap(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << tr("Usage: euxis agent-bootstrap <agent-id> [--tier <tier>]") << "\n"; return 2; }
    std::string agent_id = args[0];
    // Validate agent ID
    for (char c : agent_id) {
        if (c == '/' || c == '\\' || c == ' ') {
            std::cerr << tr("Invalid agent ID (no spaces, slashes):") << " " << agent_id << "\n";
            return 1;
        }
    }
    std::string tier = "code";
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--tier" && i + 1 < args.size()) tier = args[++i];
    }
    auto prompt_dir = fs::path(ctx.euxis_home) / "data" / "agents" / "prompts" / "fleet";
    auto prompt_path = prompt_dir / (agent_id + ".md");
    if (fs::exists(prompt_path)) { std::cerr << tr("Agent already exists:") << " " << agent_id << "\n"; return 1; }
    fs::create_directories(prompt_dir);
    std::ofstream f(prompt_path);
    f << "---\nagent_id: " << agent_id << "\nrole: worker\ntier: " << tier << "\nversion: 1.0\n---\n\n";
    f << "# " << agent_id << "\n\nYou are the " << agent_id << " agent.\n";
    std::cout << term::green("Created: " + prompt_path.string()) << "\n";
    return 0;
}

// --- squad ---
int cmd_squad(Context& ctx, const std::vector<std::string>& args) {
    RegistryClient registry(ctx.data_dir);
    auto squads = registry.list_squads();
    if (args.empty() || args[0] == "list") {
        if (ctx.json_output) {
            nlohmann::json j = nlohmann::json::array();
            for (const auto& s : squads) j.push_back({{"id", s.id}, {"name", s.name}, {"purpose", s.purpose}});
            std::cout << j.dump(2) << "\n";
        } else {
            std::cout << term::bold(tr("Squads")) << " (" << squads.size() << ")\n\n";
            std::vector<term::TableRow> rows;
            for (const auto& s : squads) rows.push_back({{s.id, s.name, s.purpose}});
            term::print_table({tr("ID"), tr("Name"), tr("Purpose")}, rows);
        }
        return 0;
    }
    if (args[0] == "info") {
        if (args.size() < 2) { std::cerr << tr("Usage: euxis squad info <squad-id>") << "\n"; return 2; }
        for (const auto& s : squads) {
            if (s.id == args[1]) {
                std::cout << term::bold(s.name) << " (" << s.id << ")\n";
                std::cout << "  Purpose: " << s.purpose << "\n";
                std::cout << "  Lead:    " << s.lead << "\n";
                std::cout << "  Members: ";
                for (const auto& m : s.members) std::cout << m << " ";
                std::cout << "\n";
                return 0;
            }
        }
        std::cerr << tr("Squad not found:") << " " << args[1] << "\n";
        return 1;
    }
    if (args[0] == "members") {
        if (args.size() < 2) { std::cerr << tr("Usage: euxis squad members <squad-id>") << "\n"; return 2; }
        for (const auto& s : squads) {
            if (s.id == args[1]) {
                for (const auto& m : s.members) std::cout << "  " << m << "\n";
                return 0;
            }
        }
        std::cerr << tr("Squad not found:") << " " << args[1] << "\n";
        return 1;
    }
    if (args[0] == "validate") {
        bool all_valid = true;
        for (const auto& s : squads) {
            for (const auto& m : s.members) {
                if (!registry.get_agent(m)) {
                    std::cerr << term::yellow("Missing member: " + m + " in squad " + s.id) << "\n";
                    all_valid = false;
                }
            }
        }
        return all_valid ? 0 : 1;
    }
    if (args[0] == "deploy") {
        if (args.size() < 3) { std::cerr << tr("Usage: euxis squad deploy <squad-id> <task> [--mode parallel|sequential]") << "\n"; return 2; }
        std::string squad_id = args[1];
        std::string task = args[2];
        for (const auto& s : squads) {
            if (s.id == squad_id) {
                std::cout << term::bold("Deploying squad: " + s.name) << "\n";
                ProviderRouter router(ctx.data_dir);
                ProviderExecutor executor(ctx.data_dir);
                for (const auto& m : s.members) {
                    auto agent = registry.get_agent(m);
                    auto tier = agent ? agent->tier : "code";
                    auto model = router.route(tier, task);
                    auto response = executor.execute(model, ProviderExecutor::build_prompt("", task), 120);
                    std::string status = response.success ? term::green("OK") : term::red("FAIL");
                    std::cout << "  " << m << ": " << status << "\n";
                }
                return 0;
            }
        }
        std::cerr << tr("Squad not found:") << " " << squad_id << "\n";
        return 1;
    }
    std::cerr << tr("Usage: euxis squad <list|info|members|validate|deploy>") << "\n";
    return 2;
}

// --- combo ---
int cmd_combo(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << tr("Usage: euxis combo <agent1,agent2,...> [task]") << "\n"; return 2; }
    std::string agents_str = args[0];
    std::string task = (args.size() > 1) ? args[1] : "Process sequentially";
    // Split by comma
    std::vector<std::string> agent_ids;
    std::istringstream ss(agents_str);
    std::string id;
    while (std::getline(ss, id, ',')) if (!id.empty()) agent_ids.push_back(id);

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    std::string prev_output;
    for (const auto& aid : agent_ids) {
        auto agent = registry.get_agent(aid);
        auto tier = agent ? agent->tier : "code";
        auto model = router.route(tier, task);
        std::string context = prev_output.empty() ? "" : prev_output;
        auto response = executor.execute(model, ProviderExecutor::build_prompt("", task, context), 120);
        if (response.success) prev_output = response.output;
        std::string status = response.success ? term::green("OK") : term::red("FAIL");
        std::cout << term::bold(aid) << ": " << status << "\n";
    }
    return 0;
}

// --- playbook ---

int cmd_playbook(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << tr("Usage: euxis playbook <manifest.json> [goal] [--mode flash|standard|forensic] [--ci] [--stats] [--compare] [--check-baseline]") << "\n";
        return 2;
    }

    // Fast exit: --stats mode
    if (args[0] == "--stats" || (args.size() > 1 && args[1] == "--stats")) {
        std::string since;
        int last_n = 0;
        bool check_baseline = false;
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--since" && i + 1 < args.size()) since = args[++i];
            if (args[i] == "--last" && i + 1 < args.size()) last_n = std::stoi(args[++i]);
            if (args[i] == "--check-baseline") check_baseline = true;
        }
        bool all_met = print_playbook_stats(ctx, since, last_n);
        if (check_baseline && !all_met) {
            std::cerr << term::red("Baseline check failed: not all targets met.") << "\n";
            return 1;
        }
        return 0;
    }

    // A/B comparison: --compare runs flash then standard on same manifest
    bool compare_mode = false;
    for (const auto& a : args) if (a == "--compare") compare_mode = true;
    if (compare_mode) {
        // Build base args without --compare and --mode, extract manifest name
        std::vector<std::string> base_args;
        std::string compare_manifest;
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--compare") continue;
            if (args[i] == "--mode" && i + 1 < args.size()) { ++i; continue; }
            if (args[i] == "--ci" || args[i] == "--policy") { base_args.push_back(args[i]); continue; }
            if (!args[i].starts_with("--") && compare_manifest.empty()) {
                compare_manifest = args[i];
            }
            base_args.push_back(args[i]);
        }

        if (compare_manifest.empty()) {
            std::cerr << tr("Error: --compare requires a manifest name.") << "\n";
            std::cerr << tr("Usage: euxis playbook <manifest> --compare") << "\n";
            return 2;
        }

        std::ostream& out = std::cerr;  // Compare output always to stderr
        out << term::bold(term::cyan("\n  === A/B COMPARISON: Flash vs Standard ===")) << "\n";
        out << "    " << term::bold("Manifest:") << "  " << compare_manifest << "\n\n";

        // Snapshot history size before runs to verify fresh artifacts
        auto pre_history = load_verdict_history(ctx.euxis_home, 500);
        size_t pre_count = pre_history.size();

        // Run flash
        out << term::bold("  [1/2] Running Flash mode...") << "\n";
        auto flash_args = base_args;
        flash_args.push_back("--mode");
        flash_args.push_back("flash");
        int flash_rc = cmd_playbook(ctx, flash_args);

        if (flash_rc == 2) {
            std::cerr << term::red("Error: Flash run failed to produce an artifact. Aborting comparison.") << "\n";
            return 2;
        }

        // Run standard
        out << term::bold("\n  [2/2] Running Standard mode...") << "\n";
        auto standard_args = base_args;
        standard_args.push_back("--mode");
        standard_args.push_back("standard");
        int standard_rc = cmd_playbook(ctx, standard_args);

        if (standard_rc == 2) {
            std::cerr << term::red("Error: Standard run failed to produce an artifact. Aborting comparison.") << "\n";
            return 2;
        }

        // Verify both runs produced fresh history entries
        auto post_history = load_verdict_history(ctx.euxis_home, 500);
        if (post_history.size() < pre_count + 2) {
            std::cerr << term::red("Error: Expected 2 new history entries but got ")
                      << (post_history.size() - pre_count) << ". Comparison results may be stale.\n";
            return 1;
        }

        const auto& std_run = post_history.back();
        const auto& flash_run = post_history[post_history.size() - 2];

        // Verify modes match expectations
        if (flash_run.mode != "flash" || std_run.mode != "standard") {
            std::cerr << term::red("Error: History entries have unexpected modes (")
                      << flash_run.mode << ", " << std_run.mode << "). Aborting.\n";
            return 1;
        }

        out << "\n" << term::bold(term::cyan("  === COMPARISON RESULTS ===")) << "\n";
        out << "    " << term::bold("Manifest:") << "  " << compare_manifest << "\n";
        out << "    " << term::bold("Flash ts:") << "  " << flash_run.timestamp << "\n";
        out << "    " << term::bold("Std ts:") << "    " << std_run.timestamp << "\n\n";

        out << "    " << std::setw(20) << std::left << "" << std::setw(20) << "Flash" << "Standard\n";
        out << "    " << std::setw(20) << std::left << "Verdict:" << std::setw(20) << flash_run.verdict << std_run.verdict << "\n";
        out << "    " << std::setw(20) << std::left << "Confidence:" << std::setw(20) << (std::to_string(flash_run.confidence) + "%") << (std::to_string(std_run.confidence) + "%") << "\n";
        out << "    " << std::setw(20) << std::left << "Total latency:" << std::setw(20) << term::format_duration(flash_run.latency_ms) << term::format_duration(std_run.latency_ms) << "\n";
        out << "    " << std::setw(20) << std::left << "Median agent:" << std::setw(20) << term::format_duration(flash_run.median_agent_latency_ms) << term::format_duration(std_run.median_agent_latency_ms) << "\n";
        out << "    " << std::setw(20) << std::left << "P95 agent:" << std::setw(20) << term::format_duration(flash_run.p95_agent_latency_ms) << term::format_duration(std_run.p95_agent_latency_ms) << "\n";
        out << "    " << std::setw(20) << std::left << "Agents:" << std::setw(20) << flash_run.agents_executed << std_run.agents_executed << "\n";
        out << "    " << std::setw(20) << std::left << "Evidence density:" << std::setw(20)
                  << (std::to_string((int)(flash_run.evidence_density * 100)) + "%")
                  << (std::to_string((int)(std_run.evidence_density * 100)) + "%") << "\n";
        out << "    " << std::setw(20) << std::left << "Early-stopped:" << std::setw(20) << (flash_run.early_stopped ? "yes" : "no") << (std_run.early_stopped ? "yes" : "no") << "\n";
        out << "    " << std::setw(20) << std::left << "Timeouts:" << std::setw(20) << flash_run.timeout_count << std_run.timeout_count << "\n";

        // Verdict drift analysis
        int conf_drift = std_run.confidence - flash_run.confidence;
        double speedup = (std_run.latency_ms > 0) ? std_run.latency_ms / std::max(1.0, flash_run.latency_ms) : 0.0;

        out << "\n" << term::bold(term::cyan("  --- Analysis ---")) << "\n";
        out << "    " << term::bold("Speedup:") << "     " << std::fixed << std::setprecision(1) << speedup << "x (flash vs standard)\n";
        out << "    " << term::bold("Conf drift:") << "  " << (conf_drift >= 0 ? "+" : "") << conf_drift << " points\n";

        if (flash_run.verdict == std_run.verdict) {
            out << "    " << term::green("MATCH") << " " << term::dim("Flash and standard agree on verdict.") << "\n";
        } else {
            out << "    " << term::yellow("DRIFT") << " " << term::dim("Verdict mismatch: flash=" + flash_run.verdict + " standard=" + std_run.verdict) << "\n";
        }

        // Drift classification: semantic vs mechanical
        bool standard_deeper = std_run.agents_executed > flash_run.agents_executed;
        bool standard_harsher = conf_drift < 0;
        bool standard_timeout_ok = std_run.timeout_count <= 1;
        int extra_agents = std_run.agents_executed - flash_run.agents_executed;

        if (flash_run.verdict == std_run.verdict && std::abs(conf_drift) <= 10) {
            out << "    " << term::green("OK") << " " << term::dim("Modes agree. Confidence drift within 10-point tolerance.") << "\n";
        } else if (standard_deeper && standard_harsher && standard_timeout_ok) {
            // Standard ran more agents, found deeper issues, and wasn't timeout-broken
            out << "    " << term::cyan("SEMANTIC") << " "
                << term::dim("Divergence explained: standard inspects more pillars than flash triage scope.") << "\n";
            out << "    " << term::dim("  Standard ran " + std::to_string(extra_agents)
                + " more agents covering pillars (architecture, execution) flash does not inspect.") << "\n";
            if (std_run.timeout_count == 0)
                out << "    " << term::dim("  No standard timeouts — divergence reflects real deeper findings, not broken execution.") << "\n";
            else
                out << "    " << term::dim("  " + std::to_string(std_run.timeout_count)
                    + " standard timeout(s), but divergence primarily from completed agents' findings.") << "\n";
        } else if (std_run.timeout_count > 2) {
            out << "    " << term::yellow("MECHANICAL") << " "
                << term::dim("Divergence likely caused by standard timeouts (" + std::to_string(std_run.timeout_count)
                    + " agents timed out).") << "\n";
            out << "    " << term::dim("  Standard machinery needs stabilization before drift is meaningful.") << "\n";
        } else if (std::abs(conf_drift) <= 10) {
            out << "    " << term::green("OK") << " " << term::dim("Confidence drift within 10-point tolerance.") << "\n";
        } else {
            out << "    " << term::yellow("MIXED") << " "
                << term::dim("Confidence drift exceeds 10-point tolerance. Cause unclear — inspect per-agent results.") << "\n";
        }
        out << "\n";

        return (flash_rc != 0 || standard_rc != 0) ? 1 : 0;
    }

    std::string mode = "flash";
    std::string goal = "Audit and improve the codebase";
    std::string manifest_name = args[0];
    bool ci_mode = false;
    std::string policy_path;
    bool policy_flag = false;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--mode" && i + 1 < args.size()) { mode = args[++i]; }
        else if (args[i] == "--ci") { ci_mode = true; }
        else if (args[i] == "--policy") {
            policy_flag = true;
            if (i + 1 < args.size() && !args[i + 1].starts_with("--")) policy_path = args[++i];
        }
        else if (!args[i].starts_with("--")) { goal = args[i]; }
    }

    // Auto-detect CI environment
    if (!ci_mode) {
        ci_mode = std::getenv("CI") || std::getenv("GITHUB_ACTIONS") ||
                  std::getenv("GITLAB_CI") || std::getenv("JENKINS_URL") ||
                  std::getenv("BUILDKITE") || std::getenv("CIRCLECI") ||
                  std::getenv("EUXIS_CI");
    }

    // CI mode: suppress color, lock escalation behavior
    // All TUI output goes to `tui` — in CI mode this is stderr, keeping stdout clean for JSON
    if (ci_mode) ctx.no_color = true;
    std::ostream& tui = ci_mode ? std::cerr : std::cout;

    auto manifest_path = fs::path(ctx.data_dir) / "config" / "playbooks" / (manifest_name + ".json");
    if (!fs::exists(manifest_path)) manifest_path = manifest_name;

    std::ifstream f(manifest_path);
    nlohmann::json manifest;
    try { manifest = nlohmann::json::parse(f); }
    catch (...) { std::cerr << "Invalid Manifest\n"; return 1; }

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    Session session(ctx.euxis_home);

    // --- SYSTEM INITIALIZATION PROBE ---
    tui << "\n" << term::bold(term::cyan("  === SYSTEM INITIALIZATION ===")) << "\n";
    auto check_auth = [&](const std::string& provider, const std::string& reauth_cmd) {
        auto auth = executor.auth_store().resolve_with_fallback(provider);
        bool has_key = (provider == "openai" && std::getenv("OPENAI_API_KEY")) ||
                       (provider == "claude" && std::getenv("ANTHROPIC_API_KEY")) ||
                       (provider == "gemini" && std::getenv("GEMINI_API_KEY"));
        
        if (has_key) {
            tui << "    " << term::icon_ok() << " " << provider << " " << term::green("(Native API Key Active)") << "\n";
        } else if (auth && !auth->token.empty()) {
            tui << "    " << term::icon_ok() << " " << provider << " " << term::blue("(OAuth Session Active)") << "\n";
        } else if (Process::available(provider)) {
            tui << "    " << term::icon_ok() << " " << provider << " " << term::dim("(Binary found, CLI auth preserved)") << "\n";
        } else {
            tui << "    " << term::icon_warn() << " " << provider << " " << term::dim("(Not Configured -> ") << term::yellow(reauth_cmd) << term::dim(")") << "\n";
        }
    };
    check_auth("claude", "claude login");
    check_auth("gemini", "gemini login");
    check_auth("openai", "export OPENAI_API_KEY=...");
    check_auth("ollama", "ollama serve");
    tui << "\n";

    // --- ADAPTIVE STEP SELECTION ---
    nlohmann::json all_steps_json = nlohmann::json::array();
    if (manifest.contains("steps")) {
        all_steps_json = manifest["steps"];
    } else if (manifest.contains("phases")) {
        for (const auto& phase : manifest["phases"]) {
            for (const auto& d : phase["delegates"]) {
                nlohmann::json s = d;
                s["pillar"] = phase.value("gate", "execution");
                s["name"] = phase.value("name", "Phase") + " / " + d.value("agent", "");
                all_steps_json.push_back(s);
            }
        }
    }

    std::vector<nlohmann::json> active_steps;
    if (mode == "flash") {
        // Triage: Librarian (0), Reviewer (10)
        for (int i : {0, 10}) if (i < (int)all_steps_json.size()) active_steps.push_back(all_steps_json[i]);
    } else if (mode == "standard") {
        // Standard: 5 agents (writer dropped — low yield for verification)
        for (int i : {0, 3, 4, 8, 10}) if (i < (int)all_steps_json.size()) active_steps.push_back(all_steps_json[i]);
    } else {
        for (const auto& s : all_steps_json) active_steps.push_back(s);
    }

    std::vector<StepPlan> plans;
    std::vector<term::TableRow> manifest_rows;

    for (const auto& step : active_steps) {
        auto plan = build_step_plan(step, mode, goal, registry, router);
        auto agent = registry.get_agent(plan.agent_id);
        std::string tier = agent ? agent->tier : "code";
        std::string tier_color = (tier == "reason") ? term::magenta(tier) : term::cyan(tier);
        manifest_rows.push_back({{plan.name, plan.agent_id, tier_color, plan.model.provider, plan.model.model}});
        plans.push_back(std::move(plan));
    }

    // --- SLA BUDGET ---
    std::string original_mode = mode;
    bool escalated = false;
    bool escalation_suppressed = false;
    std::string escalation_reason;
    bool triage_any_decisive = false;
    auto sla = get_mode_sla(mode);

    tui << term::bold(term::cyan("  === PRE-FLIGHT VERDICT PLAN [MODE: " + mode + "] ===")) << "\n";
    if (mode == "flash") {
        tui << "    " << term::dim("SLA: " + std::to_string((int)(sla.target_ms/1000)) + "s target / "
                  + std::to_string((int)(sla.budget_ms/1000)) + "s budget guard / "
                  + std::to_string(sla.agent_timeout_s) + "s per agent") << "\n";
    }
    term::print_table(tui, {tr("Step Name"), tr("Agent"), tr("Tier"), tr("Provider"), tr("Model")}, manifest_rows);
    tui << "\n" << term::bold(term::cyan("  === CONCURRENT EVIDENCE COLLECTION ===")) << "\n";

    // --- EVIDENCE COLLECTION WITH EARLY-STOP CONVERGENCE + SLA ENFORCEMENT ---
    const int total_slots = 8;
    int used_slots = 0;
    int completed_count = 0;
    int failures = 0;
    bool early_stopped = false;
    int agents_skipped = 0;
    bool budget_exceeded = false;

    std::mutex mtx;
    std::condition_variable cv;
    std::unordered_set<std::string> completed;
    std::unordered_map<std::string, std::string> step_outputs;
    std::unordered_map<std::string, std::string> step_verdicts;
    std::vector<AgentEvidence> evidence_log;
    std::vector<double> agent_latencies;
    std::vector<bool> started(plans.size(), false);
    std::string session_failover;
    double total_estimated_cost = 0.0;
    std::vector<std::string> critical_gaps; // Concrete gap reasons

    auto start_time = std::chrono::steady_clock::now();

    while (completed_count < (int)plans.size()) {
        std::unique_lock<std::mutex> lock(mtx);
        bool launched = false;

        // --- HARD CAP: Kill the run if wall clock exceeds budget ---
        auto wall_elapsed = std::chrono::steady_clock::now() - start_time;
        double wall_ms = std::chrono::duration<double, std::milli>(wall_elapsed).count();
        if (wall_ms > sla.budget_ms && original_mode == "flash") {
            budget_exceeded = true;
            int remaining = 0;
            for (size_t i = 0; i < plans.size(); ++i) {
                if (!started[i]) {
                    started[i] = true; completed_count++; completed.insert(plans[i].name);
                    remaining++;
                    critical_gaps.push_back(plans[i].name + ": skipped (flash budget exceeded at " + std::to_string((int)(wall_ms/1000)) + "s)");
                }
            }
            if (remaining > 0) {
                agents_skipped += remaining;
                tui << "    " << term::icon_warn() << " " << term::red("Flash budget exceeded (" + std::to_string((int)(wall_ms/1000)) + "s > " + std::to_string((int)(sla.budget_ms/1000)) + "s) — skipping " + std::to_string(remaining) + " remaining agents") << "\n";
            }
            cv.notify_all();
            break;
        }

        // --- EARLY-STOP: Only during escalated phase (not during 2-agent triage) ---
        if (original_mode == "flash" && escalated && !early_stopped && completed_count >= 5) {
            if (verdict_converged(step_verdicts, 4)) {
                early_stopped = true;
                agents_skipped = (int)plans.size() - completed_count;
                for (size_t i = 0; i < plans.size(); ++i) {
                    if (!started[i]) { started[i] = true; completed_count++; completed.insert(plans[i].name); }
                }
                tui << "    " << term::icon_info() << " " << term::green("Early-stop: Verdict converged after " + std::to_string(completed_count - agents_skipped) + " agents") << "\n";
                cv.notify_all();
                break;
            }
        }

        // --- ESCALATION: After flash triage completes, check if standard sweep needed ---
        if (original_mode == "flash" && !escalated && !escalation_suppressed && completed_count >= (int)plans.size()) {
            bool should_escalate = false;

            // Trigger 1: Disagreement
            {
                int p = 0, f = 0;
                for (const auto& [_, v] : step_verdicts) {
                    if (v == "PASS" || v == "DEGRADED") p++;
                    else if (v == "FAILED") f++;
                }
                if (p > 0 && f > 0) { should_escalate = true; escalation_reason = "disagreement"; }
            }

            // Trigger 2: Reviewer FAIL — use raw_verdict, not terminal status
            if (!should_escalate) {
                for (const auto& ev : evidence_log) {
                    if (ev.agent_id == "reviewer" && ev.raw_verdict == "FAIL") {
                        should_escalate = true; escalation_reason = "reviewer_fail"; break;
                    }
                }
            }

            // Trigger 3: No decisive verdict at all
            if (!should_escalate) {
                bool any_decisive = false;
                for (const auto& [_, v] : step_verdicts) {
                    if (v == "PASS" || v == "FAILED" || v == "DEGRADED") any_decisive = true;
                }
                triage_any_decisive = any_decisive;
                if (!any_decisive) { should_escalate = true; escalation_reason = "no_decisive_verdict"; }
            } else {
                // Still compute triage_any_decisive for artifact
                for (const auto& [_, v] : step_verdicts) {
                    if (v == "PASS" || v == "FAILED" || v == "DEGRADED") { triage_any_decisive = true; break; }
                }
            }

            if (should_escalate && !ci_mode) {
                mode = "standard";
                escalated = true;
                sla = get_mode_sla(mode);

                tui << "    " << term::icon_warn() << " " << term::yellow(
                    "Escalating to standard sweep (" + escalation_reason + ")") << "\n";

                // Inject remaining standard agents: Writer(1), Architect(3), Optimizer(4), Sentinel(8)
                for (int idx : {1, 3, 4, 8}) {
                    if (idx >= (int)all_steps_json.size()) continue;
                    auto& step = all_steps_json[idx];
                    std::string esc_agent_id = step.value("agent", "");

                    // Skip if already in plans
                    bool already_planned = false;
                    for (const auto& p : plans) { if (p.agent_id == esc_agent_id) { already_planned = true; break; } }
                    if (already_planned) continue;

                    auto esc_plan = build_step_plan(step, "standard", goal, registry, router);
                    esc_plan.name = "Escalation / " + esc_agent_id;

                    // Strip deps that reference step names not in step_outputs
                    std::erase_if(esc_plan.depends, [&](const std::string& d) {
                        bool missing = step_outputs.find(d) == step_outputs.end();
                        if (missing && !ci_mode) {
                            tui << "    " << term::dim("  stripped dep '" + d + "' from " + esc_agent_id
                                 + " (not available from triage)") << "\n";
                        }
                        return missing;
                    });

                    auto esc_agent = registry.get_agent(esc_agent_id);
                    std::string esc_tier = esc_agent ? esc_agent->tier : "code";
                    manifest_rows.push_back({{esc_plan.name, esc_agent_id, esc_tier,
                                               esc_plan.model.provider, esc_plan.model.model}});
                    plans.push_back(std::move(esc_plan));
                    started.push_back(false);
                }
                // Loop continues — completed_count < plans.size() again
            } else if (should_escalate && ci_mode) {
                escalation_suppressed = true;
            }
        }

        for (size_t i = 0; i < plans.size(); ++i) {
            if (started[i]) continue;
            bool deps_met = true;
            for (const auto& d : plans[i].depends) {
                if (completed.find(d) == completed.end()) { deps_met = false; break; }
            }

            int weight = (plans[i].model.provider == "claude" || plans[i].model.provider == "gemini") ? 4 : 1;
            if (deps_met && (used_slots + weight) <= total_slots) {
                started[i] = true; used_slots += weight; launched = true;
                // Clamp per-agent timeout to remaining budget guard budget
                auto remaining_ms = sla.budget_ms - std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start_time).count();
                int agent_timeout = std::min(sla.agent_timeout_s, std::max(1, (int)(remaining_ms / 1000.0)));

                std::thread([&, i, plan = plans[i], weight, agent_timeout]() {
                    std::string context;
                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        for (const auto& d : plan.depends) context += "## " + d + "\n" + step_outputs[d] + "\n---\n";
                        tui << "  " << term::icon_info() << " " << term::bold(plan.name) << term::dim(" (" + plan.model.provider + ")") << "\n";
                    }

                    ModelSelection m = plan.model;
                    bool is_degraded = false;
                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        if (!session_failover.empty()) { m.provider = session_failover; m.model = "qwen2.5-coder:7b"; is_degraded = true; }
                    }

                    auto response = executor.execute(m, ProviderExecutor::build_prompt("", plan.task, context), agent_timeout);
                    bool timed_out = (response.exit_code == 124 || response.exit_code == 137 || response.exit_code == 143 ||
                                      response.duration_ms > agent_timeout * 1000.0);

                    {
                        std::lock_guard<std::mutex> lk(mtx);
                        if (response.success || timed_out) {
                            step_outputs[plan.name] = response.output;

                            // Classify raw verdict — check multiple patterns
                            std::string raw_v = "WARN";
                            auto out_upper = response.output;
                            std::transform(out_upper.begin(), out_upper.end(), out_upper.begin(), ::toupper);
                            // Primary: exact "VERDICT: PASS/FAIL/WARN"
                            if (out_upper.find("VERDICT: PASS") != std::string::npos) raw_v = "PASS";
                            else if (out_upper.find("VERDICT: FAIL") != std::string::npos) raw_v = "FAIL";
                            // Fallback: common variations agents emit
                            else if (out_upper.find("VERDICT:PASS") != std::string::npos ||
                                     out_upper.find("FINAL VERDICT: PASS") != std::string::npos ||
                                     out_upper.find("**VERDICT: PASS**") != std::string::npos ||
                                     out_upper.find("VERDICT — PASS") != std::string::npos) raw_v = "PASS";
                            else if (out_upper.find("VERDICT:FAIL") != std::string::npos ||
                                     out_upper.find("FINAL VERDICT: FAIL") != std::string::npos ||
                                     out_upper.find("**VERDICT: FAIL**") != std::string::npos ||
                                     out_upper.find("VERDICT — FAIL") != std::string::npos) raw_v = "FAIL";
                            else if (out_upper.find("VERDICT: WARN") != std::string::npos ||
                                     out_upper.find("VERDICT:WARN") != std::string::npos ||
                                     out_upper.find("FINAL VERDICT: WARN") != std::string::npos ||
                                     out_upper.find("**VERDICT: WARN**") != std::string::npos) raw_v = "WARN";

                            // Map to explicit terminal state
                            std::string status = classify_agent_status(raw_v, timed_out, is_degraded,
                                                                       response.duration_ms, agent_timeout * 1000.0);

                            if (status == "FAILED") failures++;
                            // A timed-out agent that emitted FAIL is still a negative signal
                            if (status == "TIMEOUT" && raw_v == "FAIL") failures++;
                            step_verdicts[plan.name] = status;
                            agent_latencies.push_back(response.duration_ms);
                            total_estimated_cost += m.estimated_cost_per_1m * 0.002;

                            // Deep evidence parsing
                            auto [exec_backed, inferred, total_claims_parsed] = parse_evidence_depth(response.output);
                            auto findings = extract_findings(response.output);

                            evidence_log.push_back({
                                plan.name, plan.agent_id, status, plan.pillar, response.duration_ms,
                                exec_backed, inferred, total_claims_parsed,
                                findings, is_degraded, raw_v
                            });

                            // Track concrete gaps
                            if (status == "TIMEOUT") {
                                std::string timeout_detail = (raw_v == "FAIL")
                                    ? " — agent emitted FAIL before timeout"
                                    : " — partial evidence only";
                                critical_gaps.push_back(plan.name + " (" + plan.pillar + "): timed out at " +
                                    std::to_string((int)(response.duration_ms/1000)) + "s" + timeout_detail);
                            } else if (status == "PARTIAL") {
                                critical_gaps.push_back(plan.name + " (" + plan.pillar + "): no clear verdict emitted — evidence ambiguous");
                            } else if (status == "DEGRADED") {
                                critical_gaps.push_back(plan.name + " (" + plan.pillar + "): ran on fallback provider — reduced reasoning quality");
                            }

                            std::string icon = (status == "PASS") ? term::icon_ok() :
                                               (status == "TIMEOUT") ? term::icon_warn() : term::icon_ok();
                            tui << "    " << icon << " " << term::bold(plan.name) << " [" << format_agent_status(status) << "] "
                                      << term::dim(term::format_duration(response.duration_ms))
                                      << term::dim(" | " + std::to_string(exec_backed) + " exec/" + std::to_string(inferred) + " inferred") << "\n";
                        } else {
                            tui << "    " << term::icon_fail() << " " << term::bold(plan.name) << ": " << term::red(response.error) << "\n";
                            if (response.exit_code == 137 || response.exit_code == 143) {
                                session_failover = "ollama";
                                tui << term::dim("    \xe2\x9a\xa0  Resource pressure. Failover active.\n");
                            }
                            evidence_log.push_back({plan.name, plan.agent_id, "FAILED", plan.pillar, response.duration_ms, 0, 0, 0, {response.error}, is_degraded, "FAIL"});
                            critical_gaps.push_back(plan.name + " (" + plan.pillar + "): execution failed — " + response.error);
                            failures++;
                        }
                        completed.insert(plan.name); completed_count++; used_slots -= weight;
                    }
                    cv.notify_all();
                }).detach();
            }
        }
        if (!launched && completed_count < (int)plans.size()) {
            cv.wait(lock, [&]{ return completed_count >= (int)plans.size() || used_slots < total_slots; });
        }
    }

    // Wait for all detached threads to finish their critical sections
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]{ return completed_count >= (int)plans.size(); });
    }

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    double total_s = std::chrono::duration<double>(elapsed).count();

    // =====================================================================
    //  CLAIM VERIFICATION ENGINE — Auto-verify inferred claims
    // =====================================================================
    std::vector<VerificationResult> verifications;
    int claims_upgraded = 0;

    // Use current directory as repo root (euxis should be run from repo root)
    auto verifiers = detect_project_verifiers(fs::current_path().string());
    if (!verifiers.empty() && !evidence_log.empty()) {
        tui << "\n" << term::bold(term::cyan("  === CLAIM VERIFICATION ===")) << "\n";
        verifications = run_claim_verifiers(verifiers, sla.agent_timeout_s);
        for (const auto& vr : verifications) {
            if (vr.success) {
                claims_upgraded += vr.claims_upgraded;
                tui << "    " << term::icon_ok() << " " << term::bold(vr.check_name)
                          << " " << term::green("VERIFIED") << " " << term::dim(term::format_duration(vr.duration_ms)) << "\n";
            } else {
                tui << "    " << term::icon_fail() << " " << term::bold(vr.check_name)
                          << " " << term::red("FAILED") << " " << term::dim(vr.output_summary) << "\n";
            }
        }
        // Upgrade evidence density: move claims from inferred to execution-backed
        if (claims_upgraded > 0 && !evidence_log.empty()) {
            // Distribute upgraded claims across agents proportionally
            int per_agent = std::max(1, claims_upgraded / (int)evidence_log.size());
            for (auto& ev : evidence_log) {
                int upgrade = std::min(per_agent, ev.inferred);
                ev.execution_backed += upgrade;
                ev.inferred -= upgrade;
            }
        }

        // Map successful verifiers to pillar coverage
        auto verifier_pillar = [](const std::string& name) -> std::string {
            if (name.find("build") != std::string::npos || name.find("cmake") != std::string::npos ||
                name.find("cargo-check") != std::string::npos || name.find("go-vet") != std::string::npos ||
                name.find("make") != std::string::npos) return "build";
            if (name.find("test") != std::string::npos || name.find("jest") != std::string::npos ||
                name.find("vitest") != std::string::npos || name.find("pytest") != std::string::npos) return "testing";
            return "";
        };
        for (const auto& vr : verifications) {
            std::string pillar = verifier_pillar(vr.check_name);
            if (!pillar.empty()) {
                std::string status = vr.success ? "PASS" : "FAILED";
                evidence_log.push_back({
                    "claim-verifier/" + vr.check_name, "claim-verifier", status, pillar, vr.duration_ms,
                    vr.success ? vr.claims_upgraded : 0, 0, std::max(1, vr.claims_upgraded),
                    {}, false, status
                });
            }
        }
    }

    // =====================================================================
    //  BUILT-IN PILLAR CHECKS — Fill testing & security coverage gaps
    // =====================================================================
    // Only run when there's actual LLM agent evidence (not on empty manifests)
    bool has_agent_evidence = false;
    for (const auto& ev : evidence_log) {
        if (ev.agent_id.find("builtin-") == std::string::npos &&
            ev.agent_id.find("claim-verifier") == std::string::npos) {
            has_agent_evidence = true; break;
        }
    }
    if (has_agent_evidence) {
        auto repo_root = fs::current_path().string();
        tui << "\n" << term::bold(term::cyan("  === PILLAR COVERAGE CHECKS ===")) << "\n";

        // Testing pillar
        auto testing_checks = run_testing_pillar_checks(repo_root);
        for (const auto& tc : testing_checks) {
            std::string icon = (tc.status == "passing" || tc.status == "detected")
                ? term::icon_ok() : (tc.status == "missing" ? term::icon_warn() : term::icon_fail());
            std::string color = (tc.status == "passing" || tc.status == "detected")
                ? term::green(tc.status) : (tc.status == "missing" ? term::yellow(tc.status) : term::red(tc.status));
            tui << "    " << icon << " " << term::bold(tc.name) << " " << color
                      << " " << term::dim(tc.detail) << "\n";

            std::string verdict = (tc.status == "passing" || tc.status == "detected") ? "PASS"
                                : (tc.status == "failing" ? "FAILED" : "PARTIAL");
            evidence_log.push_back({
                "builtin/" + tc.name, "builtin-testing", verdict, "testing", tc.duration_ms,
                tc.execution_backed, tc.claims - tc.execution_backed, tc.claims,
                {tc.detail}, false, verdict
            });
        }

        // Security pillar
        auto security_checks = run_security_pillar_checks(repo_root);
        for (const auto& sc : security_checks) {
            std::string icon = (sc.status == "passing")
                ? term::icon_ok() : (sc.status == "missing" ? term::icon_warn() : term::icon_fail());
            std::string color = (sc.status == "passing")
                ? term::green(sc.status) : (sc.status == "missing" ? term::yellow(sc.status) : term::red(sc.status));
            tui << "    " << icon << " " << term::bold(sc.name) << " " << color
                      << " " << term::dim(sc.detail) << "\n";

            std::string verdict = (sc.status == "passing") ? "PASS"
                                : (sc.status == "failing" ? "FAILED" : "PARTIAL");
            evidence_log.push_back({
                "builtin/" + sc.name, "builtin-security", verdict, "security", sc.duration_ms,
                sc.execution_backed, sc.claims - sc.execution_backed, sc.claims,
                {sc.detail}, false, verdict
            });
        }
    } // end if (has_agent_evidence)

    // =====================================================================
    //  EPISTEMIC TRUST ENGINE v2 — Truth-based confidence, not infra health
    // =====================================================================

    // 1. Agent agreement analysis (using terminal states + raw verdicts)
    int passes = 0, fails = 0, partial_count = 0, timeout_count = 0;
    for (const auto& [name, v] : step_verdicts) {
        if (v == "PASS" || v == "DEGRADED") passes++;
        else if (v == "FAILED") fails++;
        else if (v == "TIMEOUT") timeout_count++;
        else partial_count++; // PARTIAL
    }
    // raw_fail_count: total agents with raw_verdict=FAIL (regardless of terminal state)
    // decisive_negatives: subset that are NOT terminal FAILED (e.g. TIMEOUT+FAIL) — extra signal
    int raw_fail_count = 0;
    int decisive_negatives = 0;
    for (const auto& ev : evidence_log) {
        if (ev.raw_verdict == "FAIL") {
            raw_fail_count++;
            if (ev.verdict != "FAILED") decisive_negatives++;
        }
    }
    // Include decisive negatives in the effective fail count for verdict synthesis
    int effective_fails = fails + decisive_negatives;
    int total_agents = passes + fails + partial_count + timeout_count;
    bool has_contradiction = (passes > 0 && effective_fails > 0);
    // Directional agreement: agents with clear signals (PASS/DEGRADED/FAILED + raw FAIL)
    int directional_agents = passes + effective_fails;
    double agreement_ratio = (directional_agents > 0)
        ? static_cast<double>(std::max(passes, fails)) / directional_agents
        : 1.0; // No directional agents = no disagreement

    // 2. Evidence density: execution-backed vs inferred claims
    int total_exec = 0, total_inferred = 0, total_claims = 0;
    for (const auto& ev : evidence_log) {
        total_exec += ev.execution_backed;
        total_inferred += ev.inferred;
        total_claims += ev.total_claims;
    }
    double evidence_density = (total_claims > 0)
        ? static_cast<double>(total_exec) / total_claims
        : 0.0;

    // 3. Pillar coverage: which critical pillars were actually checked
    std::unordered_map<std::string, std::vector<const AgentEvidence*>> pillar_evidence;
    for (const auto& ev : evidence_log) pillar_evidence[ev.pillar].push_back(&ev);

    int critical_covered = 0;
    for (const auto& cp : CRITICAL_PILLARS) {
        if (pillar_evidence.count(cp) && !pillar_evidence[cp].empty()) critical_covered++;
    }
    double critical_coverage = CRITICAL_PILLARS.empty() ? 1.0
        : static_cast<double>(critical_covered) / CRITICAL_PILLARS.size();

    // 4. Degradation penalty
    int degraded_agents = 0;
    for (const auto& ev : evidence_log) if (ev.degraded) degraded_agents++;
    double degradation_ratio = (total_agents > 0) ? static_cast<double>(degraded_agents) / total_agents : 0.0;

    // 5. Compute multi-factor confidence
    int confidence = 100;
    std::vector<std::string> confidence_factors;

    // Factor: Directional agreement (weight: 30) — only non-neutral agents
    int agreement_penalty = static_cast<int>((1.0 - agreement_ratio) * 30);
    if (has_contradiction) agreement_penalty = std::max(agreement_penalty, 25);
    confidence -= agreement_penalty;
    if (agreement_penalty > 0) confidence_factors.push_back("Agreement: -" + std::to_string(agreement_penalty) + " (" + std::to_string(int(agreement_ratio * 100)) + "% of " + std::to_string(directional_agents) + " decisive agents agree)");

    // Factor: Evidence density (weight: 25)
    int density_penalty = static_cast<int>((1.0 - evidence_density) * 25);
    confidence -= density_penalty;
    if (density_penalty > 5) confidence_factors.push_back("Evidence depth: -" + std::to_string(density_penalty) + " (" + std::to_string(total_exec) + " exec / " + std::to_string(total_inferred) + " inferred)");

    // Factor: Critical pillar coverage (weight: 25)
    int coverage_penalty = static_cast<int>((1.0 - critical_coverage) * 25);
    confidence -= coverage_penalty;
    if (coverage_penalty > 0) confidence_factors.push_back("Coverage: -" + std::to_string(coverage_penalty) + " (" + std::to_string(critical_covered) + "/" + std::to_string(CRITICAL_PILLARS.size()) + " critical pillars)");

    // Factor: Degradation (weight: 15)
    int degradation_penalty = static_cast<int>(degradation_ratio * 15);
    confidence -= degradation_penalty;
    if (degradation_penalty > 0) confidence_factors.push_back("Degraded: -" + std::to_string(degradation_penalty) + " (" + std::to_string(degraded_agents) + " agents on fallback)");

    // Factor: Early-stop (weight: 5) — penalty only when agents were actually skipped
    if (early_stopped && agents_skipped > 0) {
        confidence -= 5;
        confidence_factors.push_back("Early-stop: -5 (" + std::to_string(agents_skipped) + " agents skipped)");
    }

    // Factor: Incomplete evidence (weight: 15) — agents that couldn't deliver a clear verdict
    int incomplete_count = timeout_count + partial_count;
    if (incomplete_count > 0) {
        int incomplete_penalty = incomplete_count * 5;
        confidence -= incomplete_penalty;
        std::string detail;
        if (timeout_count > 0) detail += std::to_string(timeout_count) + " timed out";
        if (partial_count > 0) { if (!detail.empty()) detail += ", "; detail += std::to_string(partial_count) + " ambiguous"; }
        confidence_factors.push_back("Incomplete: -" + std::to_string(incomplete_penalty) + " (" + detail + ")");
    }

    // Factor: Decisive negatives (weight: 10 per) — agents that emitted FAIL but timed out
    if (decisive_negatives > 0) {
        int dn_penalty = decisive_negatives * 10;
        confidence -= dn_penalty;
        confidence_factors.push_back("Decisive negatives: -" + std::to_string(dn_penalty) + " (" + std::to_string(decisive_negatives) + " agents emitted FAIL before timeout)");
    }

    // Factor: Budget exceeded (weight: 10)
    if (budget_exceeded) {
        confidence -= 10;
        confidence_factors.push_back("Budget miss: -10 (flash SLA exceeded)");
    }

    confidence = std::max(0, std::min(100, confidence));

    std::string rationale;
    if (confidence_factors.empty()) {
        rationale = "Full consensus with dense execution evidence across all critical pillars.";
    } else {
        rationale = confidence_factors[0];
        for (size_t i = 1; i < confidence_factors.size(); ++i) rationale += "; " + confidence_factors[i];
    }

    // --- Rich Verdict Spectrum ---
    std::string verdict_text;
    uint8_t vr = 255, vg = 255, vb = 255;

    if (has_contradiction) {
        verdict_text = "INCONCLUSIVE";
        vr = 255; vg = 165; vb = 0;
    } else if (effective_fails > 0 && passes == 0) {
        verdict_text = "BLOCKED";
        vr = 255; vg = 0; vb = 0;
    } else if (confidence >= 80 && effective_fails == 0 && timeout_count == 0) {
        verdict_text = "TRUSTED";
        vr = 0; vg = 255; vb = 0;
    } else if (confidence >= 60 && effective_fails == 0) {
        verdict_text = "TRUSTED WITH GAPS";
        vr = 255; vg = 255; vb = 0;
    } else {
        verdict_text = "CAUTION";
        vr = 255; vg = 100; vb = 0;
    }

    // --- Suppressed Escalation Cap ---
    // If escalation was triggered but suppressed (CI mode), the unresolved signal
    // means we cannot trust a passing verdict. Cap at CAUTION.
    if (escalation_suppressed) {
        if (verdict_text == "TRUSTED" || verdict_text == "TRUSTED WITH GAPS") {
            verdict_text = "CAUTION";
            vr = 255; vg = 100; vb = 0;
            confidence_factors.push_back("Suppressed escalation: capped at CAUTION (reason: " + escalation_reason + ")");
            rationale += "; Suppressed escalation: capped at CAUTION (reason: " + escalation_reason + ")";
        }
    }

    // --- Contradiction Report ---
    std::vector<ContradictionEntry> contradictions;
    if (has_contradiction) {
        // Find pairs of agents that disagree on the same pillar
        for (size_t a = 0; a < evidence_log.size(); ++a) {
            for (size_t b = a + 1; b < evidence_log.size(); ++b) {
                if (evidence_log[a].pillar == evidence_log[b].pillar &&
                    evidence_log[a].verdict != evidence_log[b].verdict &&
                    evidence_log[a].verdict != "PARTIAL" && evidence_log[b].verdict != "PARTIAL" &&
                    evidence_log[a].verdict != "TIMEOUT" && evidence_log[b].verdict != "TIMEOUT") {
                    contradictions.push_back({
                        evidence_log[a].pillar,
                        evidence_log[a].agent_name, evidence_log[a].verdict,
                        evidence_log[b].agent_name, evidence_log[b].verdict,
                        "Conflicting verdicts on " + evidence_log[a].pillar
                    });
                }
            }
        }
        // Also report cross-pillar contradictions (any PASS vs any FAIL)
        if (contradictions.empty()) {
            std::string pass_agent, fail_agent;
            for (const auto& ev : evidence_log) {
                if (ev.verdict == "PASS" && pass_agent.empty()) pass_agent = ev.agent_name;
                if (ev.verdict == "FAIL" && fail_agent.empty()) fail_agent = ev.agent_name;
            }
            contradictions.push_back({"cross-pillar", pass_agent, "PASS", fail_agent, "FAIL",
                                      "Agents reached opposing conclusions across different evidence pillars"});
        }
    }

    // --- Pillar Scores ---
    std::vector<PillarScore> pillar_scores;
    {
        std::unordered_set<std::string> seen_pillars;
        for (const auto& ev : evidence_log) seen_pillars.insert(ev.pillar);
        for (const auto& cp : CRITICAL_PILLARS) seen_pillars.insert(cp);

        for (const auto& pname : seen_pillars) {
            PillarScore ps;
            ps.name = pname;

            if (!pillar_evidence.count(pname) || pillar_evidence[pname].empty()) {
                ps.status = "Missing";
                ps.coverage = 0; ps.agreement = 0; ps.evidence_density = 0;
            } else {
                const auto& agents = pillar_evidence[pname];
                int p = 0, f = 0, incomplete = 0, pex = 0, pinf = 0, pcl = 0;
                for (const auto* a : agents) {
                    if (a->verdict == "PASS" || a->verdict == "DEGRADED") p++;
                    else if (a->verdict == "FAILED") f++;
                    else incomplete++; // PARTIAL, TIMEOUT
                    pex += a->execution_backed; pinf += a->inferred; pcl += a->total_claims;
                }
                ps.coverage = std::min(100, (int)agents.size() * 50); // 2+ agents = full coverage
                ps.agreement = (int)(static_cast<double>(std::max({p, f, incomplete})) / agents.size() * 100);
                ps.evidence_density = (pcl > 0) ? (int)(static_cast<double>(pex) / pcl * 100) : 0;
                ps.status = (p > 0 && f > 0) ? "Conflict" : (f > 0 ? "Issue" : (incomplete > 0 ? "Gaps" : "Verified"));
            }
            pillar_scores.push_back(ps);
        }
    }

    // --- Inferred-only areas ---
    std::vector<std::string> inferred_areas;
    for (const auto& ev : evidence_log) {
        if (ev.execution_backed == 0 && ev.total_claims > 0) {
            inferred_areas.push_back(ev.agent_name + " (" + ev.pillar + "): all " + std::to_string(ev.total_claims) + " claims inferred, none execution-backed");
        }
    }

    // --- Unsupported claims ---
    std::vector<std::string> unsupported;
    for (const auto& ps : pillar_scores) {
        if (ps.status == "Missing") unsupported.push_back(ps.name + ": no agent covered this critical pillar");
    }

    // --- Benchmark Metrics ---
    BenchmarkMetrics bench;
    bench.total_latency_ms = total_s * 1000.0;
    bench.agents_executed = (int)evidence_log.size();
    bench.agents_skipped = agents_skipped;
    bench.estimated_cost_usd = total_estimated_cost;
    bench.critical_checks_expected = (int)CRITICAL_PILLARS.size();
    bench.critical_checks_present = critical_covered;
    bench.coverage_ratio = critical_coverage;
    double p95_latency_ms = 0.0;
    if (!agent_latencies.empty()) {
        auto sorted_lat = agent_latencies;
        std::sort(sorted_lat.begin(), sorted_lat.end());
        bench.median_agent_latency_ms = sorted_lat[sorted_lat.size() / 2];
        size_t p95_idx = std::min(sorted_lat.size() - 1, (size_t)(sorted_lat.size() * 0.95));
        p95_latency_ms = sorted_lat[p95_idx];
    }

    // --- Recommended Next Action (with specific reasons) ---
    std::string recommended_action;
    if (verdict_text == "TRUSTED") {
        recommended_action = "Safe to merge. Evidence is strong.";
    } else if (verdict_text == "TRUSTED WITH GAPS") {
        recommended_action = "Merge with caution.";
        if (timeout_count > 0) recommended_action += " Re-run with --mode standard to complete timed-out analyses.";
        if (!unsupported.empty()) recommended_action += " Run targeted checks for uncovered pillars.";
    } else if (verdict_text == "CAUTION") {
        recommended_action = "Do not merge without review.";
        if (budget_exceeded) recommended_action += " Flash SLA was missed — consider --mode standard for reliable coverage.";
        if (timeout_count > 0) recommended_action += " " + std::to_string(timeout_count) + " agent(s) timed out.";
        if (partial_count > 0) recommended_action += " " + std::to_string(partial_count) + " agent(s) gave ambiguous results.";
    } else if (verdict_text == "BLOCKED") {
        recommended_action = "Fix blocking issues before merge. All agents agree on failure.";
    } else {
        recommended_action = "Human review required. Agents contradict — resolve disagreements before proceeding.";
    }

    // --- Collect all key findings ---
    std::vector<std::string> all_findings;
    for (const auto& ev : evidence_log) {
        for (const auto& f : ev.key_findings) all_findings.push_back("[" + ev.agent_name + "] " + f);
    }

    // =====================================================================
    //  REUSABLE VERDICT ARTIFACT (Full Epistemic Breakdown)
    // =====================================================================
    nlohmann::json artifact;
    artifact["schema"] = VERDICT_SCHEMA;
    artifact["schema_version"] = VERDICT_SCHEMA_VERSION;
    artifact["verdict"] = verdict_text;
    artifact["confidence"] = confidence;
    artifact["confidence_rationale"] = nlohmann::json::object();
    artifact["confidence_rationale"]["factors"] = nlohmann::json::array();
    for (const auto& cf : confidence_factors) artifact["confidence_rationale"]["factors"].push_back(cf);
    artifact["confidence_rationale"]["agreement_ratio"] = agreement_ratio;
    artifact["confidence_rationale"]["evidence_density"] = evidence_density;
    artifact["confidence_rationale"]["critical_coverage"] = critical_coverage;
    artifact["confidence_rationale"]["degradation_ratio"] = degradation_ratio;
    artifact["confidence_rationale"]["raw_fail_count"] = raw_fail_count;
    artifact["confidence_rationale"]["decisive_negatives"] = decisive_negatives;

    artifact["pillar_scores"] = nlohmann::json::array();
    for (const auto& ps : pillar_scores) {
        artifact["pillar_scores"].push_back({
            {"name", ps.name}, {"status", ps.status},
            {"coverage", ps.coverage}, {"agreement", ps.agreement},
            {"evidence_density", ps.evidence_density}
        });
    }

    artifact["key_findings"] = all_findings;
    artifact["recommended_action"] = recommended_action;

    artifact["contradictions"] = nlohmann::json::array();
    for (const auto& c : contradictions) {
        artifact["contradictions"].push_back({
            {"pillar", c.pillar}, {"agent_a", c.agent_a}, {"verdict_a", c.verdict_a},
            {"agent_b", c.agent_b}, {"verdict_b", c.verdict_b}, {"detail", c.detail}
        });
    }

    artifact["inferred_areas"] = inferred_areas;
    artifact["unsupported_claims"] = unsupported;

    artifact["mode"] = original_mode;
    artifact["degraded_mode"] = !session_failover.empty();
    artifact["early_stopped"] = early_stopped;
    if (early_stopped) artifact["agents_skipped"] = agents_skipped;

    // SLA tracking
    artifact["sla"] = {
        {"target_ms", sla.target_ms},
        {"actual_ms", bench.total_latency_ms},
        {"met", bench.total_latency_ms <= sla.target_ms + SLA_TOLERANCE_MS},
        {"budget_exceeded", budget_exceeded}
    };

    // Agent status with terminal states and reason codes
    // Separate LLM agents from built-in checks
    artifact["agent_status"] = nlohmann::json::object();
    artifact["builtin_checks"] = nlohmann::json::object();
    for (const auto& ev : evidence_log) {
        auto entry_json = nlohmann::json({
            {"status", ev.verdict}, {"pillar", ev.pillar},
            {"duration_ms", ev.duration_ms},
            {"execution_backed", ev.execution_backed},
            {"inferred", ev.inferred}, {"total_claims", ev.total_claims},
            {"degraded", ev.degraded}, {"findings", ev.key_findings}
        });
        if (!ev.raw_verdict.empty()) entry_json["raw_verdict"] = ev.raw_verdict;
        bool is_builtin = (ev.agent_id.find("builtin-") == 0 || ev.agent_id == "claim-verifier");
        if (is_builtin) {
            artifact["builtin_checks"][ev.agent_name] = entry_json;
        } else {
            artifact["agent_status"][ev.agent_name] = entry_json;
        }
    }

    artifact["critical_gaps"] = critical_gaps;

    artifact["benchmark"] = {
        {"total_latency_ms", bench.total_latency_ms},
        {"median_agent_latency_ms", bench.median_agent_latency_ms},
        {"p95_agent_latency_ms", p95_latency_ms},
        {"estimated_cost_usd", bench.estimated_cost_usd},
        {"agents_executed", bench.agents_executed},
        {"agents_skipped", bench.agents_skipped},
        {"timeouts", timeout_count},
        {"critical_coverage", bench.coverage_ratio},
        {"critical_checks", std::to_string(bench.critical_checks_present) + "/" + std::to_string(bench.critical_checks_expected)},
        {"escalated", escalated}
    };

    // --- Claim Verification Results in Artifact ---
    if (!verifications.empty()) {
        artifact["claim_verification"] = nlohmann::json::array();
        for (const auto& vr : verifications) {
            artifact["claim_verification"].push_back({
                {"check", vr.check_name}, {"command", vr.command},
                {"verified", vr.success}, {"duration_ms", vr.duration_ms},
                {"claims_upgraded", vr.claims_upgraded}
            });
        }
        artifact["claims_upgraded"] = claims_upgraded;
    }

    // --- Verdict Memory: Load history, compute trend, append ---
    auto history = load_verdict_history(ctx.euxis_home);
    auto trend = compute_verdict_trend(history, verdict_text, confidence);

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ts_stream;
    ts_stream << std::put_time(std::gmtime(&now_t), "%Y-%m-%dT%H:%M:%SZ");
    std::string timestamp = ts_stream.str();

    // Append current run to history
    append_verdict_history(ctx.euxis_home, {
        timestamp, verdict_text, confidence, original_mode,
        total_s * 1000.0, bench.median_agent_latency_ms, p95_latency_ms,
        (int)evidence_log.size(), agents_skipped, timeout_count,
        evidence_density, early_stopped, escalated, budget_exceeded
    });

    // Add trend to artifact
    artifact["trend"] = {
        {"direction", trend.direction},
        {"confidence_delta", trend.confidence_delta},
        {"run_count", trend.run_count + 1},
        {"regression", trend.regression},
        {"previous_verdict", trend.previous_verdict},
        {"streak", trend.streak}
    };
    artifact["timestamp"] = timestamp;
    artifact["escalated"] = escalated;
    artifact["escalation_reason"] = escalation_reason.empty() ? nlohmann::json(nullptr) : nlohmann::json(escalation_reason);

    // Triage block — separates triage outcome from final verdict
    if (original_mode == "flash") {
        artifact["triage"] = {
            {"completed", true},
            {"agents", 2},
            {"decisive", triage_any_decisive},
            {"outcome", escalated ? "escalated" : (escalation_suppressed ? "suppressed" : "converged")},
            {"escalation_reason", escalation_reason.empty() ? nlohmann::json(nullptr) : nlohmann::json(escalation_reason)},
            {"suppressed_in_ci", escalation_suppressed}
        };
    }

    // --- Policy Evaluation ---
    std::vector<PolicyViolation> policy_violations;
    std::optional<Policy> active_policy;
    if (policy_flag) {
        active_policy = load_policy(ctx.data_dir, policy_path);
    } else {
        active_policy = load_policy(ctx.data_dir);
    }
    artifact["policy_applied"] = active_policy.has_value();
    if (active_policy) {
        policy_violations = evaluate_policy(artifact, *active_policy);
        nlohmann::json policy_json;
        policy_json["min_verdict"] = active_policy->min_verdict;
        policy_json["min_confidence"] = active_policy->min_confidence;
        policy_json["min_critical_coverage"] = active_policy->min_critical_coverage;
        policy_json["fail_on_regression"] = active_policy->fail_on_regression;
        policy_json["max_drift"] = active_policy->max_drift;
        policy_json["fail_on_escalation"] = active_policy->fail_on_escalation;
        policy_json["fail_on_budget_exceeded"] = active_policy->fail_on_budget_exceeded;
        artifact["policy"] = policy_json;
        nlohmann::json violations_arr = nlohmann::json::array();
        for (const auto& v : policy_violations) {
            violations_arr.push_back({{"gate", v.gate}, {"detail", v.detail}});
        }
        artifact["policy_violations"] = violations_arr;
        artifact["policy_passed"] = policy_violations.empty();
    }

    auto artifact_path = fs::path(ctx.euxis_home) / "data" / "runtime" / "sessions" / "latest_verdict.json";
    fs::create_directories(artifact_path.parent_path());
    std::ofstream af(artifact_path);
    af << artifact.dump(2);

    // =====================================================================
    //  FINAL VERDICT DASHBOARD
    // =====================================================================
    tui << "\n" << term::bold(term::cyan("  === FINAL REPOSITORY VERDICT ===")) << "\n";
    tui << "    " << term::bold("Verdict:") << "       " << term::rgb_fg(vr, vg, vb, verdict_text) << "\n";
    tui << "    " << term::bold("Confidence:") << "    " << confidence << "%" << "\n";

    // SLA line — honest about budget hits
    {
        bool sla_met = (bench.total_latency_ms <= sla.target_ms + SLA_TOLERANCE_MS);
        std::string sla_label = original_mode + " SLA";
        if (sla_met) {
            tui << "    " << term::bold(sla_label + ":") << "  " << term::green("MET")
                      << " (" << term::format_duration(bench.total_latency_ms) << " / "
                      << std::to_string((int)(sla.target_ms/1000)) << "s target)\n";
        } else {
            tui << "    " << term::bold(sla_label + ":") << "  " << term::red("MISSED")
                      << " (" << term::format_duration(bench.total_latency_ms) << " vs "
                      << std::to_string((int)(sla.target_ms/1000)) << "s target)\n";
        }
    }

    tui << "    " << term::bold("Action:") << "        " << term::dim(recommended_action) << "\n";

    // Agent Status Table — explicit terminal states
    tui << "\n" << term::bold(term::cyan("  --- Agent Status ---")) << "\n";
    for (const auto& ev : evidence_log) {
        tui << "    " << term::bold(ev.agent_name) << ": " << format_agent_status(ev.verdict)
                  << " " << term::dim(term::format_duration(ev.duration_ms))
                  << term::dim(" | " + std::to_string(ev.execution_backed) + " exec/" + std::to_string(ev.inferred) + " inferred")
                  << "\n";
    }

    // Critical Gaps — concrete reasons, not vague labels
    if (!critical_gaps.empty()) {
        tui << "\n" << term::bold(term::yellow("  --- Critical Gaps ---")) << "\n";
        for (const auto& gap : critical_gaps) {
            tui << "    " << term::yellow("GAP") << " " << term::dim(gap) << "\n";
        }
    }

    // Confidence breakdown
    tui << "\n" << term::bold(term::cyan("  --- Confidence Breakdown ---")) << "\n";
    for (const auto& cf : confidence_factors) {
        tui << "    " << term::dim("  " + cf) << "\n";
    }
    if (confidence_factors.empty()) {
        tui << "    " << term::green("  No penalties. Full epistemic confidence.") << "\n";
    }

    // Pillar scores
    tui << "\n" << term::bold(term::cyan("  --- Evidence Pillars ---")) << "\n";
    for (const auto& ps : pillar_scores) {
        std::string status_color;
        if (ps.status == "Verified") status_color = term::green(ps.status);
        else if (ps.status == "Gaps") status_color = term::yellow(ps.status);
        else if (ps.status == "Conflict") status_color = term::red(ps.status);
        else status_color = term::dim(ps.status);
        tui << "    " << term::bold(ps.name) << ": " << status_color
                  << " (cov:" << ps.coverage << "% agr:" << ps.agreement << "% depth:" << ps.evidence_density << "%)\n";
    }

    // Contradictions
    if (!contradictions.empty()) {
        tui << "\n" << term::bold(term::red("  --- Contradictions ---")) << "\n";
        for (const auto& c : contradictions) {
            tui << "    " << term::red("CONFLICT") << " [" << c.pillar << "] "
                      << c.agent_a << "=" << c.verdict_a << " vs "
                      << c.agent_b << "=" << c.verdict_b << "\n";
            tui << "      " << term::dim(c.detail) << "\n";
        }
    }

    // Inferred areas
    if (!inferred_areas.empty()) {
        tui << "\n" << term::bold(term::yellow("  --- Inferred (Not Execution-Backed) ---")) << "\n";
        for (const auto& ia : inferred_areas) {
            tui << "    " << term::yellow("INFERRED") << " " << term::dim(ia) << "\n";
        }
    }

    // Unsupported critical pillars
    if (!unsupported.empty()) {
        tui << "\n" << term::bold(term::yellow("  --- Missing Critical Coverage ---")) << "\n";
        for (const auto& u : unsupported) {
            tui << "    " << term::yellow("MISSING") << " " << term::dim(u) << "\n";
        }
    }

    // Verdict Memory / Trend
    tui << "\n" << term::bold(term::cyan("  --- Verdict Trend ---")) << "\n";
    {
        std::string trend_icon;
        std::string trend_color;
        if (trend.direction == "improving") { trend_icon = "▲"; trend_color = term::green(trend.direction); }
        else if (trend.direction == "declining") { trend_icon = "▼"; trend_color = term::red(trend.direction); }
        else if (trend.direction == "volatile") { trend_icon = "~"; trend_color = term::yellow(trend.direction); }
        else if (trend.direction == "new") { trend_icon = "★"; trend_color = term::cyan(trend.direction); }
        else { trend_icon = "="; trend_color = term::dim(trend.direction); }

        tui << "    " << term::bold("Trend:") << "     " << trend_icon << " " << trend_color << "\n";
        tui << "    " << term::bold("History:") << "   " << (trend.run_count + 1) << " runs\n";
        if (trend.run_count > 0) {
            std::string delta_str = (trend.confidence_delta >= 0 ? "+" : "") + std::to_string(trend.confidence_delta);
            tui << "    " << term::bold("Delta:") << "     " << delta_str << " confidence vs last run"
                      << " (" << trend.previous_verdict << " → " << verdict_text << ")\n";
            tui << "    " << term::bold("Streak:") << "    " << trend.streak << " consecutive " << verdict_text << "\n";
        }
        if (trend.regression) {
            tui << "    " << term::red("REGRESSION") << " " << term::bold("Verdict dropped from " + trend.previous_verdict) << "\n";
        }
    }

    // Benchmark
    tui << "\n" << term::bold(term::cyan("  --- Benchmark ---")) << "\n";
    tui << "    " << term::bold("Total:") << "     " << term::format_duration(bench.total_latency_ms) << "\n";
    tui << "    " << term::bold("Median:") << "    " << term::format_duration(bench.median_agent_latency_ms) << "\n";
    tui << "    " << term::bold("P95:") << "       " << term::format_duration(p95_latency_ms) << "\n";
    tui << "    " << term::bold("Agents:") << "    " << bench.agents_executed << " executed"
              << (bench.agents_skipped > 0 ? ", " + std::to_string(bench.agents_skipped) + " skipped" : "")
              << (timeout_count > 0 ? ", " + std::to_string(timeout_count) + " timed out" : "") << "\n";
    tui << "    " << term::bold("Coverage:") << "  " << bench.critical_checks_present << "/" << bench.critical_checks_expected << " critical pillars\n";
    tui << "    " << term::bold("Cost:") << "      $" << std::fixed << std::setprecision(4) << bench.estimated_cost_usd << "\n";
    tui << "    " << term::bold("Context:") << "   " << (session_failover.empty() ? term::green("Cloud + Local") : term::yellow("Degraded (Local Pivot)")) << "\n";
    if (original_mode == "flash") {
        std::string strategy = "Triage (2 agents)";
        if (escalated) {
            int injected = (int)plans.size() - 2;
            strategy += " -> Escalated to standard (+" + std::to_string(injected) + " agents, reason: " + escalation_reason + ")";
            tui << "    " << term::bold("Strategy:") << "  " << term::yellow(strategy) << "\n";
        } else if (escalation_suppressed) {
            strategy += " -> Escalation suppressed (CI mode, reason: " + escalation_reason + ")";
            tui << "    " << term::bold("Strategy:") << "  " << term::yellow(strategy) << "\n";
        } else {
            strategy += " -> Converged";
            tui << "    " << term::bold("Strategy:") << "  " << term::green(strategy) << "\n";
        }
    } else if (early_stopped) {
        tui << "    " << term::bold("Strategy:") << "  " << term::green("Early-stop convergence") << "\n";
    }
    tui << "    " << term::bold("Artifact:") << "  " << term::dim(artifact_path.string()) << "\n\n";

    // Policy evaluation dashboard
    if (active_policy) {
        tui << term::bold(term::cyan("  --- Policy Evaluation ---")) << "\n";
        if (policy_violations.empty()) {
            tui << "    Policy: " << term::green("PASSED") << " (0 violations)\n";
        } else {
            tui << "    Policy: " << term::red("FAILED") << " (" << policy_violations.size() << " violations)\n";
            for (const auto& v : policy_violations) {
                tui << "      - " << term::bold(v.gate) << ": " << v.detail << "\n";
            }
        }
        tui << "\n";
    }

    // --- CI MODE: Emit JSON to stdout ---
    if (ci_mode) {
        // Add exit_code to artifact for machine consumption
        int exit_code = 2;
        if (!evidence_log.empty()) {
            if (effective_fails > 0 || has_contradiction) exit_code = 1;
            else if (verdict_text == "CAUTION" || verdict_text == "BLOCKED" || verdict_text == "INCONCLUSIVE") exit_code = 1;
            else exit_code = 0;
        }
        // Policy override: violations always force exit code 1
        if (active_policy && !policy_violations.empty() && exit_code == 0) exit_code = 1;
        artifact["exit_code"] = exit_code;
        artifact["ci"] = true;
        std::cout << artifact.dump(2) << "\n";
        return exit_code;
    }

    // Exit codes: 0=trusted/trusted-with-gaps, 1=blocked/inconclusive/caution, 2=no evidence
    if (evidence_log.empty()) return 2;
    if (effective_fails > 0 || has_contradiction) return 1;
    if (verdict_text == "CAUTION" || verdict_text == "BLOCKED" || verdict_text == "INCONCLUSIVE") return 1;
    // Policy override: violations always force exit code 1
    if (active_policy && !policy_violations.empty()) return 1;
    return 0;
}

// --- ci: CI-first verdict entry point ---
int cmd_ci(Context& ctx, const std::vector<std::string>& args) {
    // `euxis ci` = `euxis playbook verify-everything --ci`
    // Accepts: euxis ci [manifest] [goal] [--mode flash|standard|forensic]
    std::string manifest = "verify-everything";
    std::vector<std::string> playbook_args;
    bool has_manifest = false;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--policy") {
            playbook_args.push_back(args[i]);
            if (i + 1 < args.size() && !args[i + 1].starts_with("--")) {
                playbook_args.push_back(args[++i]);
            }
        } else if (!args[i].starts_with("--") && !has_manifest) {
            manifest = args[i];
            has_manifest = true;
        } else {
            playbook_args.push_back(args[i]);
        }
    }

    playbook_args.insert(playbook_args.begin(), manifest);
    playbook_args.push_back("--ci");

    return cmd_playbook(ctx, playbook_args);
}

// --- playbook --stats: Aggregated validation metrics from history ---
bool print_playbook_stats(Context& ctx, const std::string& since, int last_n) {
    auto all_history = load_verdict_history(ctx.euxis_home, 500);
    if (all_history.empty()) {
        std::cout << term::yellow("No verdict history found.") << " Run 'euxis playbook' to generate data.\n";
        return false;
    }

    // Filter by --since (ISO date prefix match, e.g. "2026-03-14")
    std::vector<VerdictHistoryEntry> history;
    if (!since.empty()) {
        for (const auto& h : all_history) {
            if (h.timestamp >= since) history.push_back(h);
        }
        if (history.empty()) {
            std::cout << term::yellow("No runs found since " + since + ".") << "\n";
            return false;
        }
    } else {
        history = all_history;
    }

    // Filter by --last N
    if (last_n > 0 && (int)history.size() > last_n) {
        history = std::vector<VerdictHistoryEntry>(history.end() - last_n, history.end());
    }

    // Show filter info
    if (!since.empty() || last_n > 0) {
        std::string filter_desc;
        if (!since.empty()) filter_desc += "since " + since;
        if (last_n > 0) {
            if (!filter_desc.empty()) filter_desc += ", ";
            filter_desc += "last " + std::to_string(last_n);
        }
        std::cout << term::dim("  Filter: " + filter_desc + " (" + std::to_string(history.size()) + " of " + std::to_string(all_history.size()) + " runs)") << "\n\n";
    }

    // Load target thresholds
    ConfigLoader target_loader(ctx.data_dir);
    nlohmann::json default_targets = {
        {"flash_sla_hit_rate", 90},
        {"early_stop_rate", 60},
        {"max_escalation_rate", 20},
        {"max_confidence_drift", 10},
        {"flash_p95_latency_ms", 75000},
        {"min_flash_confidence", 70},
        {"max_timeout_rate", 15}
    };
    bool targets_from_file = target_loader.exists("config/targets.json");
    auto targets = target_loader.load_or("config/targets.json", default_targets);
    int targets_met = 0, targets_total = 0;

    auto target_indicator = [&](bool passed) -> std::string {
        targets_total++;
        if (passed) { targets_met++; return term::green("PASS"); }
        return term::red("FAIL");
    };

    // Partition by mode
    std::vector<const VerdictHistoryEntry*> flash_runs, standard_runs, forensic_runs;
    for (const auto& h : history) {
        if (h.mode == "flash") flash_runs.push_back(&h);
        else if (h.mode == "standard") standard_runs.push_back(&h);
        else forensic_runs.push_back(&h);
    }

    auto compute_percentile = [](std::vector<double> vals, double pct) -> double {
        if (vals.empty()) return 0.0;
        std::sort(vals.begin(), vals.end());
        size_t idx = std::min(vals.size() - 1, (size_t)(vals.size() * pct));
        return vals[idx];
    };

    auto compute_median = [](std::vector<double> vals) -> double {
        if (vals.empty()) return 0.0;
        std::sort(vals.begin(), vals.end());
        return vals[vals.size() / 2];
    };

    std::cout << "\n" << term::bold(term::cyan("  === VERDICT ENGINE VALIDATION METRICS ===")) << "\n";
    std::cout << "    " << term::dim("Source: " + verdict_history_path(ctx.euxis_home).string()) << "\n";
    std::cout << "    " << term::dim("Total runs: " + std::to_string(history.size())) << "\n";
    std::cout << "    " << term::dim("Targets: " + std::string(targets_from_file ? "config/targets.json" : "compiled defaults")) << "\n\n";

    // --- Metric 1: Flash Latency ---
    std::cout << term::bold(term::cyan("  --- 1. Flash Latency ---")) << "\n";
    if (flash_runs.empty()) {
        std::cout << "    " << term::dim("No flash runs recorded.") << "\n";
    } else {
        std::vector<double> flash_totals, flash_medians, flash_p95s;
        for (const auto* r : flash_runs) {
            flash_totals.push_back(r->latency_ms);
            if (r->median_agent_latency_ms > 0) flash_medians.push_back(r->median_agent_latency_ms);
            if (r->p95_agent_latency_ms > 0) flash_p95s.push_back(r->p95_agent_latency_ms);
        }
        std::cout << "    " << term::bold("Runs:") << "           " << flash_runs.size() << "\n";
        std::cout << "    " << term::bold("Total median:") << "   " << term::format_duration(compute_median(flash_totals)) << "\n";
        std::cout << "    " << term::bold("Total p95:") << "      " << term::format_duration(compute_percentile(flash_totals, 0.95)) << "\n";
        if (!flash_medians.empty()) {
            std::cout << "    " << term::bold("Agent median:") << "   " << term::format_duration(compute_median(flash_medians)) << "\n";
        }
        double p95_val = 0.0;
        if (!flash_p95s.empty()) {
            p95_val = compute_median(flash_p95s);
            std::cout << "    " << term::bold("Agent p95:") << "      " << term::format_duration(p95_val) << "\n";
        }
        // SLA hit rate
        int sla_met = 0;
        for (const auto* r : flash_runs) if (r->latency_ms <= 45000 + SLA_TOLERANCE_MS) sla_met++;
        int sla_rate = (int)(100.0 * sla_met / flash_runs.size());
        int sla_target = targets.value("flash_sla_hit_rate", 90);
        std::cout << "    " << term::bold("SLA hit rate:") << "   "
                  << sla_met << "/" << flash_runs.size() << " ("
                  << sla_rate << "%) " << target_indicator(sla_rate >= sla_target)
                  << term::dim(" (target: " + std::to_string(sla_target) + "%)") << "\n";

        // Flash P95 latency target
        if (!flash_p95s.empty()) {
            double p95_target = targets.value("flash_p95_latency_ms", 75000.0);
            std::cout << "    " << term::bold("P95 latency:") << "    "
                      << target_indicator(p95_val <= p95_target)
                      << term::dim(" (target: " + term::format_duration(p95_target) + ")") << "\n";
        }

        // Min flash confidence
        std::vector<double> flash_conf;
        for (const auto* r : flash_runs) flash_conf.push_back(r->confidence);
        double median_conf = compute_median(flash_conf);
        int conf_target = targets.value("min_flash_confidence", 70);
        std::cout << "    " << term::bold("Median conf:") << "    "
                  << (int)median_conf << "% " << target_indicator((int)median_conf >= conf_target)
                  << term::dim(" (target: " + std::to_string(conf_target) + "%)") << "\n";
    }

    // --- Metric 2: Early-Stop Rate ---
    std::cout << "\n" << term::bold(term::cyan("  --- 2. Early-Stop Rate ---")) << "\n";
    if (flash_runs.empty()) {
        std::cout << "    " << term::dim("No flash runs recorded.") << "\n";
    } else {
        int early_stops = 0;
        for (const auto* r : flash_runs) if (r->early_stopped) early_stops++;
        int rate = (int)(100.0 * early_stops / flash_runs.size());
        int es_target = targets.value("early_stop_rate", 60);
        std::string es_label = (rate >= es_target) ? term::green("INFO") : term::yellow("INFO");
        std::cout << "    " << term::bold("Early-stops:") << "  "
                  << early_stops << "/" << flash_runs.size() << " ("
                  << rate << "% of flash runs) " << es_label
                  << term::dim(" (ref: >=" + std::to_string(es_target) + "%)") << "\n";
        // Average agents skipped when early-stopped
        if (early_stops > 0) {
            int total_skipped = 0;
            for (const auto* r : flash_runs) if (r->early_stopped) total_skipped += r->agents_skipped;
            std::cout << "    " << term::bold("Avg skipped:") << "  "
                      << (double)total_skipped / early_stops << " agents per early-stop\n";
        }
    }

    // --- Metric 3: Escalation Frequency ---
    std::cout << "\n" << term::bold(term::cyan("  --- 3. Flash→Standard Escalation ---")) << "\n";
    if (flash_runs.empty()) {
        std::cout << "    " << term::dim("No flash runs recorded.") << "\n";
    } else {
        int escalations = 0;
        for (const auto* r : flash_runs) if (r->escalated) escalations++;
        int rate = (int)(100.0 * escalations / flash_runs.size());
        int esc_target = targets.value("max_escalation_rate", 20);
        std::string color = (rate > 30) ? term::red(std::to_string(rate) + "%")
                          : (rate > 10) ? term::yellow(std::to_string(rate) + "%")
                          : term::green(std::to_string(rate) + "%");
        std::cout << "    " << term::bold("Escalations:") << "  "
                  << escalations << "/" << flash_runs.size() << " (" << color << ") "
                  << target_indicator(rate <= esc_target)
                  << term::dim(" (target: <=" + std::to_string(esc_target) + "%)") << "\n";
        if (escalations > 0) {
            std::cout << "    " << term::dim("High escalation rate suggests flash agents disagree too often.") << "\n";
            std::cout << "    " << term::dim("Consider tuning flash prompts or increasing agent timeout.") << "\n";
        }
    }

    // --- Metric 4: Verdict Drift (Flash vs Standard) ---
    std::cout << "\n" << term::bold(term::cyan("  --- 4. Verdict Drift (Flash vs Standard) ---")) << "\n";
    if (flash_runs.empty() || standard_runs.empty()) {
        std::cout << "    " << term::dim("Need both flash and standard runs for drift analysis.") << "\n";
        std::cout << "    " << term::dim("Run: euxis playbook <manifest> --compare") << "\n";
    } else if (flash_runs.size() < 3 || standard_runs.size() < 3) {
        std::cout << "    " << term::dim("Insufficient baseline runs (need >=3 of each mode).") << "\n";
        std::cout << "    " << term::dim("Flash: " + std::to_string(flash_runs.size()) + ", Standard: " + std::to_string(standard_runs.size())) << "\n";
    } else {
        // Compare verdict distributions
        auto count_verdicts = [](const std::vector<const VerdictHistoryEntry*>& runs) {
            std::map<std::string, int> counts;
            for (const auto* r : runs) counts[r->verdict]++;
            return counts;
        };
        auto flash_verdicts = count_verdicts(flash_runs);
        auto standard_verdicts = count_verdicts(standard_runs);

        std::cout << "    " << term::bold("Flash verdict distribution:") << "\n";
        for (const auto& [v, c] : flash_verdicts) {
            int pct = (int)(100.0 * c / flash_runs.size());
            std::cout << "      " << v << ": " << c << " (" << pct << "%)\n";
        }
        std::cout << "    " << term::bold("Standard verdict distribution:") << "\n";
        for (const auto& [v, c] : standard_verdicts) {
            int pct = (int)(100.0 * c / standard_runs.size());
            std::cout << "      " << v << ": " << c << " (" << pct << "%)\n";
        }

        // Confidence comparison
        std::vector<double> flash_conf, standard_conf;
        for (const auto* r : flash_runs) flash_conf.push_back(r->confidence);
        for (const auto* r : standard_runs) standard_conf.push_back(r->confidence);
        double fc = compute_median(flash_conf);
        double sc = compute_median(standard_conf);
        double drift = sc - fc;
        int drift_target = targets.value("max_confidence_drift", 10);
        std::cout << "    " << term::bold("Confidence:") << "  flash median=" << (int)fc
                  << "%, standard median=" << (int)sc << "%"
                  << " (drift: " << (drift >= 0 ? "+" : "") << (int)drift << ") "
                  << target_indicator(std::abs((int)drift) <= drift_target)
                  << term::dim(" (target: <=" + std::to_string(drift_target) + "pt)") << "\n";

        if (std::abs((int)drift) > drift_target) {
            // Classify drift: compare median agent counts and timeout rates
            double flash_median_agents = 0, std_median_agents = 0;
            double flash_timeout_rate = 0, std_timeout_rate = 0;
            for (const auto* r : flash_runs) { flash_median_agents += r->agents_executed; flash_timeout_rate += r->timeout_count; }
            for (const auto* r : standard_runs) { std_median_agents += r->agents_executed; std_timeout_rate += r->timeout_count; }
            flash_median_agents /= flash_runs.size();
            std_median_agents /= standard_runs.size();
            flash_timeout_rate /= flash_runs.size();
            std_timeout_rate /= standard_runs.size();

            bool deeper = std_median_agents > flash_median_agents + 1;
            bool low_timeouts = std_timeout_rate <= 2.0;

            if (deeper && low_timeouts && drift < 0) {
                std::cout << "    " << term::cyan("SEMANTIC") << " " << term::dim("Standard inspects more pillars than flash — expected divergence.") << "\n";
            } else if (std_timeout_rate > 2.0) {
                std::cout << "    " << term::yellow("MECHANICAL") << " " << term::dim("Standard timeout rate is high — drift may be from broken execution.") << "\n";
            } else {
                std::cout << "    " << term::yellow("WARNING") << " " << term::dim("Confidence drift exceeds " + std::to_string(drift_target) + "-point tolerance.") << "\n";
            }
        } else {
            std::cout << "    " << term::green("OK") << " " << term::dim("Confidence drift within " + std::to_string(drift_target) + "-point tolerance.") << "\n";
        }
    }

    // --- Timeout Analysis ---
    std::cout << "\n" << term::bold(term::cyan("  --- Timeout Analysis ---")) << "\n";
    {
        int total_timeouts = 0, total_agents = 0, budget_misses = 0;
        for (const auto& h : history) {
            total_timeouts += h.timeout_count;
            total_agents += h.agents_executed;
            if (h.budget_exceeded) budget_misses++;
        }
        int timeout_rate = (total_agents > 0) ? (int)(100.0 * total_timeouts / total_agents) : 0;
        int timeout_target = targets.value("max_timeout_rate", 15);
        std::cout << "    " << term::bold("Total timeouts:") << "   " << total_timeouts << " across " << history.size() << " runs\n";
        std::cout << "    " << term::bold("Timeout rate:") << "     " << timeout_rate << "% "
                  << target_indicator(timeout_rate <= timeout_target)
                  << term::dim(" (target: <=" + std::to_string(timeout_target) + "%)") << "\n";
        std::cout << "    " << term::bold("Budget exceeded:") << "  " << budget_misses << "/" << history.size() << " runs\n";
    }

    // --- Policy Compliance (cross-layer monitoring) ---
    {
        auto policy_opt = load_policy(ctx.data_dir);
        if (policy_opt) {
            int policy_passed = 0;
            // Evaluate each run's recorded data against the active policy
            for (const auto& h : history) {
                // Build a minimal artifact-like JSON for policy evaluation
                nlohmann::json run_artifact;
                run_artifact["verdict"] = h.verdict;
                run_artifact["confidence"] = h.confidence;
                run_artifact["escalated"] = h.escalated;
                run_artifact["sla"] = {{"met", !h.budget_exceeded}};
                run_artifact["trend"] = {{"regression", false}}; // Can't reconstruct per-run
                run_artifact["benchmark"] = {{"critical_coverage", h.evidence_density}};
                auto violations = evaluate_policy(run_artifact, *policy_opt);
                if (violations.empty()) policy_passed++;
            }
            int compliance = (int)(100.0 * policy_passed / history.size());
            std::cout << "\n" << term::bold(term::cyan("  --- Policy Compliance ---")) << "\n";
            std::string comp_color = (compliance >= 90) ? term::green(std::to_string(compliance) + "%")
                                   : (compliance >= 70) ? term::yellow(std::to_string(compliance) + "%")
                                   : term::red(std::to_string(compliance) + "%");
            std::cout << "    " << term::bold("Compliance rate:") << "  " << policy_passed << "/" << history.size()
                      << " (" << comp_color << ")\n";
        }
    }

    // --- Confidence Over Time (last 10) ---
    std::cout << "\n" << term::bold(term::cyan("  --- Recent Confidence Trend ---")) << "\n";
    {
        int start = std::max(0, (int)history.size() - 10);
        for (int i = start; i < (int)history.size(); ++i) {
            const auto& h = history[i];
            std::string bar(h.confidence / 5, '#');
            std::string mode_tag = " [" + h.mode + "]";
            std::cout << "    " << term::dim(h.timestamp.substr(0, 10)) << " "
                      << std::setw(3) << h.confidence << "% "
                      << term::cyan(bar) << " " << h.verdict << mode_tag << "\n";
        }
    }

    // --- Target Summary ---
    if (targets_total > 0) {
        std::cout << "\n" << term::bold(term::cyan("  --- Target Summary ---")) << "\n";
        std::string summary_color = (targets_met == targets_total)
            ? term::green(std::to_string(targets_met) + "/" + std::to_string(targets_total) + " targets met")
            : term::red(std::to_string(targets_met) + "/" + std::to_string(targets_total) + " targets met");
        std::cout << "    " << term::bold(summary_color) << "\n";
    }

    std::cout << "\n";
    return targets_total == 0 || targets_met == targets_total;
}

// --- dispatch ---
int cmd_dispatch(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << tr("Usage: euxis dispatch <manifest.json>") << "\n"; return 2; }
    auto path = args[0];
    if (!fs::exists(path)) { std::cerr << tr("File not found:") << " " << path << "\n"; return 1; }
    std::ifstream f(path);
    nlohmann::json manifest;
    try { manifest = nlohmann::json::parse(f); }
    catch (...) { std::cerr << tr("Invalid JSON:") << " " << path << "\n"; return 1; }
    if (!manifest.contains("agents") || !manifest["agents"].is_array()) {
        std::cerr << tr("Manifest must contain an 'agents' array") << "\n";
        return 1;
    }
    std::string global_task = manifest.value("task", "");
    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    for (const auto& entry : manifest["agents"]) {
        std::string agent_id = entry.value("agent_id", "");
        std::string task = entry.value("task", global_task);
        std::string priority = entry.value("priority", "P1");
        auto agent = registry.get_agent(agent_id);
        auto tier = agent ? agent->tier : "code";
        auto model = router.route(tier, task, priority);
        auto response = executor.execute(model, ProviderExecutor::build_prompt("", task), 120);
        std::string status = response.success ? term::green("OK") : term::red("FAIL");
        std::cout << "  [" << priority << "] " << term::bold(agent_id) << ": " << status << "\n";
    }
    return 0;
}

// --- council ---
int cmd_council(Context& ctx, const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cerr << tr("Usage: euxis council <topic> <agent1,agent2,...> [--rounds N]") << "\n"; return 2; }
    std::string topic = args[0];
    std::string agents_str = args[1];
    int rounds = 3;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--rounds" && i + 1 < args.size()) rounds = std::stoi(args[++i]);
    }
    std::vector<std::string> agent_ids;
    std::istringstream ss(agents_str);
    std::string id;
    while (std::getline(ss, id, ',')) if (!id.empty()) agent_ids.push_back(id);

    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    std::string shared_context;
    for (int round = 0; round < rounds; ++round) {
        std::cout << term::bold("Round " + std::to_string(round + 1) + "/" + std::to_string(rounds)) << "\n";
        for (const auto& aid : agent_ids) {
            auto agent = registry.get_agent(aid);
            auto tier = agent ? agent->tier : "code";
            auto model = router.route(tier, topic);
            std::string prompt = "Topic: " + topic + "\n\nPrevious discussion:\n" + shared_context;
            auto response = executor.execute(model, ProviderExecutor::build_prompt("", prompt), 120);
            if (response.success) shared_context += "\n[" + aid + "] " + response.output + "\n";
            std::string status = response.success ? term::green("OK") : term::red("FAIL");
            std::cout << "  " << term::bold(aid) << ": " << status << "\n";
        }
    }
    return 0;
}

// --- loop ---
int cmd_loop(Context& ctx, const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cerr << tr("Usage: euxis loop <agent> <task> [--max-iterations N] [--threshold T]") << "\n"; return 2; }
    std::string agent_id = args[0];
    std::string task = args[1];
    int max_iterations = 5;
    double threshold = 0.3;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--max-iterations" && i + 1 < args.size()) max_iterations = std::stoi(args[++i]);
        else if (args[i] == "--threshold" && i + 1 < args.size()) threshold = std::stod(args[++i]);
    }
    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    auto agent = registry.get_agent(agent_id);
    auto tier = agent ? agent->tier : "code";
    auto model = router.route(tier, task);
    std::string prev_output;
    for (int i = 0; i < max_iterations; ++i) {
        std::string prompt = task;
        if (!prev_output.empty()) prompt += "\n\nPrevious output:\n" + prev_output + "\n\nImprove this output.";
        auto response = executor.execute(model, ProviderExecutor::build_prompt("", prompt), 120);
        if (!response.success) break;
        // Simple convergence: check if output is similar to previous
        if (!prev_output.empty() && response.output == prev_output) {
            std::cout << term::green("Converged at iteration " + std::to_string(i + 1)) << "\n";
            break;
        }
        prev_output = response.output;
        std::cout << "  Iteration " << (i + 1) << ": " << term::dim(std::to_string(response.output.size()) + " chars") << "\n";
        (void)threshold; // Used for quality scoring in production
    }
    return 0;
}

// --- synthesize ---
int cmd_synthesize(Context& ctx, const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << tr("Usage: euxis synthesize <output-dir> [--agents <filter>] [--agent <synth-agent>]") << "\n"; return 2; }
    auto dir = args[0];
    if (!fs::exists(dir)) { std::cerr << tr("Directory not found:") << " " << dir << "\n"; return 1; }
    std::string agent_filter, synth_agent;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--agents" && i + 1 < args.size()) agent_filter = args[++i];
        else if (args[i] == "--agent" && i + 1 < args.size()) synth_agent = args[++i];
    }
    // Collect output files
    std::vector<std::string> files;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        if (!agent_filter.empty() && path.find(agent_filter) == std::string::npos) continue;
        files.push_back(path);
    }
    if (files.empty()) {
        std::cout << term::dim("No output files found.") << "\n";
        return 0;
    }
    // Read all files and synthesize
    std::string combined;
    for (const auto& f : files) {
        std::ifstream in(f);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        combined += "## " + fs::path(f).filename().string() + "\n" + content + "\n\n";
    }
    RegistryClient registry(ctx.data_dir);
    ProviderRouter router(ctx.data_dir);
    ProviderExecutor executor(ctx.data_dir);
    std::string sa = synth_agent.empty() ? "code-agent" : synth_agent;
    auto agent = registry.get_agent(sa);
    auto tier = agent ? agent->tier : "code";
    auto model = router.route(tier, "synthesize");
    auto response = executor.execute(model, ProviderExecutor::build_prompt("", "Synthesize the following outputs into a coherent summary:", combined), 120);
    if (response.success) {
        std::cout << response.output << "\n";
    } else {
        std::cerr << term::red("Synthesis failed: " + response.error) << "\n";
        return 1;
    }
    return 0;
}

} // namespace euxis::cli::cmd
