#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::metrics {

class PerformanceAnalyzer {
public:
    explicit PerformanceAnalyzer(const std::filesystem::path& metrics_dir = {});

    auto analyze_agent_performance(int hours_back = 24) -> nlohmann::json;
    auto analyze_delegation_patterns(int hours_back = 24) -> nlohmann::json;
    auto analyze_tool_usage_patterns(int hours_back = 24) -> nlohmann::json;
    auto generate_performance_report(int hours_back = 24) -> nlohmann::json;
    auto save_report(const nlohmann::json& report,
                     const std::string& filename = {}) -> std::string;
    auto get_agent_rankings(int hours_back = 24,
                            const std::string& metric = "success_rate")
        -> std::vector<std::pair<std::string, double>>;

    static auto calculate_percentile(std::vector<double> values, int percentile)
        -> double;

private:
    std::filesystem::path metrics_dir_;
    std::filesystem::path events_file_;
    std::filesystem::path sessions_file_;
    std::filesystem::path reports_dir_;

    auto load_events(int hours_back) -> std::vector<nlohmann::json>;
    auto load_sessions(int hours_back) -> std::vector<nlohmann::json>;
};

} // namespace euxis::metrics
