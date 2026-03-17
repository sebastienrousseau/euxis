#include "euxis/metrics/analyzer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace euxis::metrics {
namespace {

auto default_metrics_dir() -> std::filesystem::path {
    const char* home = std::getenv("EUXIS_HOME");
    if (home) return std::filesystem::path(home) / "data/runtime" / "metrics";
    const char* user_home = std::getenv("HOME");
    return std::filesystem::path(user_home ? user_home : "/tmp") /
           ".euxis" / "data/runtime" / "metrics";
}

auto now_iso() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+00:00", &tm);
    return buf;
}

auto now_formatted() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return buf;
}

auto mean(const std::vector<double>& v) -> double {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) /
           static_cast<double>(v.size());
}

auto median(std::vector<double> v) -> double {
    if (v.empty()) return 0.0;
    std::ranges::sort(v);
    auto n = v.size();
    if (n % 2 == 0)
        return (v[n / 2 - 1] + v[n / 2]) / 2.0;
    return v[n / 2];
}

} // namespace

auto PerformanceAnalyzer::calculate_percentile(std::vector<double> values,
                                               int percentile) -> double {
    if (values.empty()) return 0.0;
    std::ranges::sort(values);
    double index =
        (static_cast<double>(percentile) / 100.0) *
        static_cast<double>(values.size() - 1);
    auto lower = static_cast<size_t>(std::floor(index));
    auto upper = static_cast<size_t>(std::ceil(index));
    if (lower == upper) return values[lower];
    double weight = index - static_cast<double>(lower);
    return values[lower] * (1.0 - weight) + values[upper] * weight;
}

PerformanceAnalyzer::PerformanceAnalyzer(
    const std::filesystem::path& metrics_dir)
    : metrics_dir_(metrics_dir.empty() ? default_metrics_dir() : metrics_dir) {
    events_file_ = metrics_dir_ / "events.jsonl";
    sessions_file_ = metrics_dir_ / "sessions.jsonl";
    reports_dir_ = metrics_dir_ / "reports";
    std::filesystem::create_directories(reports_dir_);
}

auto PerformanceAnalyzer::load_events(int hours_back)
    -> std::vector<nlohmann::json> {
    (void)hours_back;
    if (!std::filesystem::exists(events_file_)) return {};

    std::vector<nlohmann::json> events;
    std::ifstream f(events_file_);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            events.push_back(nlohmann::json::parse(line));
        } catch (const std::exception&) {
            continue;
        }
    }
    return events;
}

auto PerformanceAnalyzer::load_sessions(int hours_back)
    -> std::vector<nlohmann::json> {
    (void)hours_back;
    if (!std::filesystem::exists(sessions_file_)) return {};

    std::vector<nlohmann::json> sessions;
    std::ifstream f(sessions_file_);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            sessions.push_back(nlohmann::json::parse(line));
        } catch (const std::exception&) {
            continue;
        }
    }
    return sessions;
}

auto PerformanceAnalyzer::analyze_agent_performance(int hours_back)
    -> nlohmann::json {
    auto sessions = load_sessions(hours_back);
    if (sessions.empty()) return {};

    struct AgentMetrics {
        int total{0}, successful{0}, failed{0}, warning{0};
        std::vector<double> durations;
    };
    std::unordered_map<std::string, AgentMetrics> agent_map;

    for (const auto& s : sessions) {
        auto agent_id = s.value("agent_id", "unknown");
        auto& m = agent_map[agent_id];
        m.total++;
        m.durations.push_back(s.value("duration_ms", 0.0));
        auto status = s.value("status", "");
        if (status == "SUCCESS") m.successful++;
        else if (status == "FAILURE") m.failed++;
        else if (status == "WARNING") m.warning++;
    }

    nlohmann::json result;
    for (auto& [agent_id, m] : agent_map) {
        if (m.total == 0) continue;
        result[agent_id] = {
            {"total_tasks", m.total},
            {"success_rate", static_cast<double>(m.successful) / m.total},
            {"failure_rate", static_cast<double>(m.failed) / m.total},
            {"avg_duration_ms", mean(m.durations)},
            {"median_duration_ms", median(m.durations)},
            {"p95_duration_ms", calculate_percentile(m.durations, 95)},
            {"p99_duration_ms", calculate_percentile(m.durations, 99)},
            {"min_duration_ms", *std::ranges::min_element(m.durations)},
            {"max_duration_ms", *std::ranges::max_element(m.durations)},
        };
    }
    return result;
}

auto PerformanceAnalyzer::analyze_delegation_patterns(int hours_back)
    -> nlohmann::json {
    auto events = load_events(hours_back);
    if (events.empty()) return {};

    struct PairData {
        int count{0};
        std::vector<double> durations;
        int success{0}, total{0};
    };
    std::unordered_map<std::string, PairData> pairs;

    for (const auto& e : events) {
        if (e.value("event_type", "") == "Agent:DelegationCompleted") {
            auto props = e["properties"];
            auto key = props.value("delegating_agent", "") + "->" +
                       props.value("target_agent", "");
            auto& p = pairs[key];
            p.count++;
            p.total++;
            p.durations.push_back(props.value("total_duration_ms", 0.0));
            if (props.value("handoff_successful", true)) p.success++;
        }
    }

    nlohmann::json result;
    for (auto& [key, p] : pairs) {
        if (p.total == 0) continue;
        result[key] = {
            {"delegation_frequency", p.count},
            {"handoff_success_rate", static_cast<double>(p.success) / p.total},
            {"avg_delegation_duration_ms", mean(p.durations)},
            {"p95_delegation_duration_ms",
             calculate_percentile(p.durations, 95)},
        };
    }
    return result;
}

auto PerformanceAnalyzer::analyze_tool_usage_patterns(int hours_back)
    -> nlohmann::json {
    auto events = load_events(hours_back);
    if (events.empty()) return {};

    struct ToolData {
        int total{0}, successful{0};
        std::vector<double> durations;
        int retry_events{0};
    };
    std::unordered_map<std::string, ToolData> tools;

    for (const auto& e : events) {
        if (e.value("event_type", "") == "Agent:ToolExecution") {
            auto props = e["properties"];
            auto tool = props.value("tool_name", "unknown");
            auto& t = tools[tool];
            t.total++;
            t.durations.push_back(props.value("execution_duration_ms", 0.0));
            if (props.value("success", true)) t.successful++;
            if (props.value("retries", 0) > 0) t.retry_events++;
        }
    }

    nlohmann::json result;
    for (auto& [tool, t] : tools) {
        if (t.total == 0) continue;
        result[tool] = {
            {"total_executions", t.total},
            {"success_rate", static_cast<double>(t.successful) / t.total},
            {"avg_duration_ms", mean(t.durations)},
            {"p95_duration_ms", calculate_percentile(t.durations, 95)},
            {"retry_frequency",
             static_cast<double>(t.retry_events) / t.total},
        };
    }
    return result;
}

auto PerformanceAnalyzer::generate_performance_report(int hours_back)
    -> nlohmann::json {
    auto agent_perf = analyze_agent_performance(hours_back);
    nlohmann::json report = {
        {"report_timestamp", now_iso()},
        {"analysis_period_hours", hours_back},
        {"agent_performance", agent_perf},
        {"delegation_patterns", analyze_delegation_patterns(hours_back)},
        {"tool_usage_patterns", analyze_tool_usage_patterns(hours_back)},
    };

    if (!agent_perf.empty()) {
        int total_tasks = 0;
        double total_successful = 0;
        std::string most_active, fastest;
        int most_tasks = 0;
        double lowest_avg = 1e18;

        for (auto& [agent, metrics] : agent_perf.items()) {
            int tasks = metrics.value("total_tasks", 0);
            total_tasks += tasks;
            total_successful += tasks * metrics.value("success_rate", 0.0);
            if (tasks > most_tasks) {
                most_tasks = tasks;
                most_active = agent;
            }
            double avg = metrics.value("avg_duration_ms", 1e18);
            if (avg < lowest_avg) {
                lowest_avg = avg;
                fastest = agent;
            }
        }

        report["fleet_metrics"] = {
            {"total_tasks", total_tasks},
            {"fleet_success_rate",
             total_tasks > 0 ? total_successful / total_tasks : 0.0},
            {"active_agents", static_cast<int>(agent_perf.size())},
            {"most_active_agent", most_active},
            {"fastest_agent", fastest},
        };
    }

    return report;
}

auto PerformanceAnalyzer::save_report(const nlohmann::json& report,
                                      const std::string& filename) -> std::string {
    auto name = filename.empty()
                    ? "performance_report_" + now_formatted() + ".json"
                    : filename;
    auto path = reports_dir_ / name;
    std::ofstream f(path);
    f << report.dump(2);
    return path.string();
}

auto PerformanceAnalyzer::get_agent_rankings(int hours_back,
                                             const std::string& metric)
    -> std::vector<std::pair<std::string, double>> {
    auto perf = analyze_agent_performance(hours_back);
    if (perf.empty()) return {};

    std::vector<std::pair<std::string, double>> rankings;
    for (auto& [agent, metrics] : perf.items()) {
        if (metrics.contains(metric)) {
            rankings.emplace_back(agent, metrics[metric].get<double>());
        }
    }
    std::ranges::sort(rankings,
                      [](const auto& a, const auto& b) { return a.second > b.second; });
    return rankings;
}

} // namespace euxis::metrics
