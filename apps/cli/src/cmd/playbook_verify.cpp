#include "euxis/cli/playbook_verify.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace euxis::cli::cmd {

namespace fs = std::filesystem;

std::vector<VerdictViolation> verify_verdict_consistency(
    const std::string& verdict,
    const std::vector<PlaybookEvidence>& evidence,
    int confidence,
    int contradiction_count) {
    std::vector<VerdictViolation> violations;

    int effective_fails = 0;
    int timeout_count = 0;
    int passes = 0;
    for (const auto& ev : evidence) {
        if (ev.verdict == "FAILED") effective_fails++;
        if (ev.verdict == "TIMEOUT" && ev.raw_verdict == "FAIL") effective_fails++;
        if (ev.verdict == "TIMEOUT") timeout_count++;
        if (ev.verdict == "PASS") passes++;
    }
    const bool has_contradiction = contradiction_count > 0;

    if (verdict == "TRUSTED") {
        if (has_contradiction) violations.push_back({"R1", "TRUSTED with contradictions"});
        if (effective_fails > 0) violations.push_back({"R1", "TRUSTED with effective_fails > 0"});
        if (timeout_count > 0) violations.push_back({"R1", "TRUSTED with timeouts"});
        if (confidence < 80) violations.push_back({"R1", "TRUSTED with confidence < 80"});
    }

    if (verdict == "BLOCKED" && (effective_fails == 0 || passes > 0)) {
        violations.push_back({"R2", "BLOCKED without unanimous failure"});
    }

    if (verdict == "INCONCLUSIVE" && !has_contradiction) {
        violations.push_back({"R3", "INCONCLUSIVE without contradictions"});
    }

    if (verdict == "TRUSTED" && timeout_count == static_cast<int>(evidence.size()) && !evidence.empty()) {
        violations.push_back({"R4", "TRUSTED with all agents timed out"});
    }

    if (!evidence.empty() && effective_fails > static_cast<int>(evidence.size()) / 2) {
        if (verdict == "TRUSTED" || verdict == "TRUSTED WITH GAPS") {
            violations.push_back({"R5", "Positive verdict with majority decisive negatives"});
        }
    }

    return violations;
}

std::vector<DriftAlert> detect_agent_drift(
    const std::string& euxis_home,
    const std::vector<PlaybookEvidence>& current_evidence) {
    std::vector<DriftAlert> alerts;

    auto history_path = fs::path(euxis_home) / "data" / "runtime" / "sessions" / "history.jsonl";
    if (!fs::exists(history_path)) return alerts;

    std::vector<nlohmann::json> history;
    {
        std::ifstream hf(history_path);
        std::string line;
        while (std::getline(hf, line)) {
            if (line.empty()) continue;
            try { history.push_back(nlohmann::json::parse(line)); }
            catch (...) { continue; }
        }
    }
    if (history.size() < 3) return alerts;

    if (history.size() > 10) {
        history = std::vector<nlohmann::json>(history.end() - 10, history.end());
    }

    for (const auto& ev : current_evidence) {
        std::vector<double> hist_latencies;
        int hist_timeout_count = 0;
        int hist_total = 0;

        for (const auto& h : history) {
            if (!h.contains("agent_status")) continue;
            const auto& status = h["agent_status"];
            if (!status.contains(ev.agent_name)) continue;
            const auto& agent = status[ev.agent_name];
            hist_total++;
            if (agent.contains("duration_ms")) {
                hist_latencies.push_back(agent["duration_ms"].get<double>());
            }
            if (agent.contains("status") && agent["status"] == "TIMEOUT") {
                hist_timeout_count++;
            }
        }

        if (hist_latencies.size() < 3) continue;

        double sum = 0;
        for (double l : hist_latencies) sum += l;
        const double mean = sum / static_cast<double>(hist_latencies.size());
        double var_sum = 0;
        for (double l : hist_latencies) var_sum += (l - mean) * (l - mean);
        const double stddev = std::sqrt(var_sum / static_cast<double>(hist_latencies.size()));

        if (stddev > 0 && std::abs(ev.duration_ms - mean) > 2 * stddev) {
            const double deviation = ((ev.duration_ms - mean) / mean) * 100.0;
            alerts.push_back({
                ev.agent_id, "latency", mean, ev.duration_ms, deviation,
                std::abs(deviation) > 100 ? "alert" : "warning"
            });
        }

        if (hist_total > 0) {
            const double hist_timeout_rate = static_cast<double>(hist_timeout_count) / hist_total;
            const double current_timeout = (ev.verdict == "TIMEOUT") ? 1.0 : 0.0;
            if (current_timeout > 0 && hist_timeout_rate < 0.3) {
                alerts.push_back({
                    ev.agent_id, "verdict_distribution",
                    hist_timeout_rate * 100, 100.0,
                    100.0 - hist_timeout_rate * 100, "warning"
                });
            }
        }
    }

    return alerts;
}

} // namespace euxis::cli::cmd
