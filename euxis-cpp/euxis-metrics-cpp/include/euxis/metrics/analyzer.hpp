/// @file
/// @brief Analysis and reporting for collected performance metrics.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::metrics {

/// @brief Processes raw metric streams to generate high-level performance insights.
class PerformanceAnalyzer {
public:
    /// @brief Construct analyzer targeting a specific directory.
    explicit PerformanceAnalyzer(const std::filesystem::path& metrics_dir = {});

    /// @brief Load raw events from the last N hours.
    auto load_events(int hours_back = 24) -> std::vector<nlohmann::json>;
    
    /// @brief Load session records from the last N hours.
    auto load_sessions(int hours_back = 24) -> std::vector<nlohmann::json>;

    /// @brief Compute performance statistics for each agent.
    auto analyze_agent_performance(int hours_back = 24) -> nlohmann::json;
    
    /// @brief analyze the flow of tasks between agents.
    auto analyze_delegation_patterns(int hours_back = 24) -> nlohmann::json;
    
    /// @brief analyze frequency and success rate of tool usage.
    auto analyze_tool_usage_patterns(int hours_back = 24) -> nlohmann::json;

    /// @brief generate a comprehensive performance summary report.
    auto generate_performance_report(int hours_back = 24) -> nlohmann::json;

    /// @brief Persist a report to the reports directory.
    auto save_report(const nlohmann::json& report,
                     const std::string& filename = "") -> std::string;

    /// @brief Rank agents by a specific performance metric.
    auto get_agent_rankings(int hours_back = 24, const std::string& metric = "quality_score")
        -> std::vector<std::pair<std::string, double>>;

    /// @brief Helper to calculate a percentile from a numeric series.
    static auto calculate_percentile(std::vector<double> values, int percentile)
        -> double;

private:
    std::filesystem::path metrics_dir_;
    std::filesystem::path events_file_;
    std::filesystem::path sessions_file_;
    std::filesystem::path reports_dir_;
};

} // namespace euxis::metrics
