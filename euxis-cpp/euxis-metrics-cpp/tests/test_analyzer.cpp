#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "euxis/metrics/analyzer.hpp"

namespace euxis::metrics {
namespace {

TEST(AnalyzerTest, PercentileCalculation) {
    std::vector<double> values{10, 20, 30, 40, 50};
    EXPECT_DOUBLE_EQ(PerformanceAnalyzer::calculate_percentile(values, 50), 30.0);
    EXPECT_NEAR(PerformanceAnalyzer::calculate_percentile(values, 95), 48.0, 0.1);
}

TEST(AnalyzerTest, EmptyPercentile) {
    EXPECT_DOUBLE_EQ(PerformanceAnalyzer::calculate_percentile({}, 50), 0.0);
}

TEST(AnalyzerTest, SingleValuePercentile) {
    EXPECT_DOUBLE_EQ(PerformanceAnalyzer::calculate_percentile({42.0}, 99), 42.0);
}

TEST(AnalyzerTest, EmptyDirReturnsEmptyReport) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_analyzer_test";
    std::filesystem::remove_all(tmp);
    PerformanceAnalyzer a(tmp);
    auto report = a.generate_performance_report();
    EXPECT_TRUE(report["agent_performance"].empty());
    std::filesystem::remove_all(tmp);
}

class AnalyzerWithDataTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_analyzer_data_test";
        std::filesystem::remove_all(tmp_);
        std::filesystem::create_directories(tmp_);
    }

    void TearDown() override { std::filesystem::remove_all(tmp_); }

    void write_session(const nlohmann::json& session) {
        std::ofstream f(tmp_ / "sessions.jsonl", std::ios::app);
        f << session.dump() << '\n';
    }

    void write_event(const nlohmann::json& event) {
        std::ofstream f(tmp_ / "events.jsonl", std::ios::app);
        f << event.dump() << '\n';
    }
};

TEST_F(AnalyzerWithDataTest, AnalyzeAgentPerformanceWithSessions) {
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 100}, {"status", "SUCCESS"}});
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 200}, {"status", "SUCCESS"}});
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 300}, {"status", "FAILURE"}});
    write_session({{"agent_id", "agent-2"}, {"duration_ms", 50}, {"status", "SUCCESS"}});

    PerformanceAnalyzer a(tmp_);
    auto perf = a.analyze_agent_performance();

    ASSERT_TRUE(perf.contains("agent-1"));
    ASSERT_TRUE(perf.contains("agent-2"));
    EXPECT_EQ(perf["agent-1"]["total_tasks"], 3);
    EXPECT_NEAR(perf["agent-1"]["success_rate"].get<double>(), 2.0 / 3.0, 0.01);
    EXPECT_NEAR(perf["agent-1"]["failure_rate"].get<double>(), 1.0 / 3.0, 0.01);
    EXPECT_EQ(perf["agent-2"]["total_tasks"], 1);
    EXPECT_DOUBLE_EQ(perf["agent-2"]["success_rate"].get<double>(), 1.0);
}

TEST_F(AnalyzerWithDataTest, AnalyzeAgentPerformancePercentiles) {
    for (int i = 0; i < 20; ++i) {
        write_session({{"agent_id", "agent-1"},
                       {"duration_ms", 10 * (i + 1)},
                       {"status", "SUCCESS"}});
    }

    PerformanceAnalyzer a(tmp_);
    auto perf = a.analyze_agent_performance();
    ASSERT_TRUE(perf.contains("agent-1"));
    EXPECT_TRUE(perf["agent-1"].contains("p95_duration_ms"));
    EXPECT_TRUE(perf["agent-1"].contains("p99_duration_ms"));
    EXPECT_TRUE(perf["agent-1"].contains("min_duration_ms"));
    EXPECT_TRUE(perf["agent-1"].contains("max_duration_ms"));
    EXPECT_DOUBLE_EQ(perf["agent-1"]["min_duration_ms"].get<double>(), 10.0);
    EXPECT_DOUBLE_EQ(perf["agent-1"]["max_duration_ms"].get<double>(), 200.0);
}

TEST_F(AnalyzerWithDataTest, AnalyzeDelegationPatterns) {
    write_event({{"event_type", "Agent:DelegationCompleted"},
                 {"properties", {{"delegating_agent", "agent-1"},
                                 {"target_agent", "agent-2"},
                                 {"total_duration_ms", 100},
                                 {"handoff_successful", true}}}});
    write_event({{"event_type", "Agent:DelegationCompleted"},
                 {"properties", {{"delegating_agent", "agent-1"},
                                 {"target_agent", "agent-2"},
                                 {"total_duration_ms", 200},
                                 {"handoff_successful", false}}}});

    PerformanceAnalyzer a(tmp_);
    auto patterns = a.analyze_delegation_patterns();
    ASSERT_FALSE(patterns.empty());

    auto key = "agent-1->agent-2";
    ASSERT_TRUE(patterns.contains(key));
    EXPECT_EQ(patterns[key]["delegation_frequency"], 2);
    EXPECT_NEAR(patterns[key]["handoff_success_rate"].get<double>(), 0.5, 0.01);
    EXPECT_NEAR(patterns[key]["avg_delegation_duration_ms"].get<double>(), 150.0, 0.01);
}

TEST_F(AnalyzerWithDataTest, AnalyzeToolUsagePatterns) {
    write_event({{"event_type", "Agent:ToolExecution"},
                 {"properties", {{"tool_name", "grep"},
                                 {"execution_duration_ms", 50},
                                 {"success", true},
                                 {"retries", 0}}}});
    write_event({{"event_type", "Agent:ToolExecution"},
                 {"properties", {{"tool_name", "grep"},
                                 {"execution_duration_ms", 100},
                                 {"success", false},
                                 {"retries", 2}}}});
    write_event({{"event_type", "Agent:ToolExecution"},
                 {"properties", {{"tool_name", "read"},
                                 {"execution_duration_ms", 30},
                                 {"success", true},
                                 {"retries", 0}}}});

    PerformanceAnalyzer a(tmp_);
    auto tools = a.analyze_tool_usage_patterns();
    ASSERT_FALSE(tools.empty());
    ASSERT_TRUE(tools.contains("grep"));
    ASSERT_TRUE(tools.contains("read"));

    EXPECT_EQ(tools["grep"]["total_executions"], 2);
    EXPECT_NEAR(tools["grep"]["success_rate"].get<double>(), 0.5, 0.01);
    EXPECT_NEAR(tools["grep"]["retry_frequency"].get<double>(), 0.5, 0.01);
    EXPECT_EQ(tools["read"]["total_executions"], 1);
    EXPECT_DOUBLE_EQ(tools["read"]["success_rate"].get<double>(), 1.0);
}

TEST_F(AnalyzerWithDataTest, GeneratePerformanceReportWithData) {
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 100}, {"status", "SUCCESS"}});
    write_session({{"agent_id", "agent-2"}, {"duration_ms", 50}, {"status", "SUCCESS"}});

    write_event({{"event_type", "Agent:DelegationCompleted"},
                 {"properties", {{"delegating_agent", "agent-1"},
                                 {"target_agent", "agent-2"},
                                 {"total_duration_ms", 150},
                                 {"handoff_successful", true}}}});

    PerformanceAnalyzer a(tmp_);
    auto report = a.generate_performance_report();

    EXPECT_TRUE(report.contains("report_timestamp"));
    EXPECT_TRUE(report.contains("agent_performance"));
    EXPECT_TRUE(report.contains("delegation_patterns"));
    EXPECT_TRUE(report.contains("tool_usage_patterns"));
    EXPECT_TRUE(report.contains("fleet_metrics"));

    EXPECT_EQ(report["fleet_metrics"]["total_tasks"], 2);
    EXPECT_EQ(report["fleet_metrics"]["active_agents"], 2);
    EXPECT_FALSE(report["fleet_metrics"]["most_active_agent"].get<std::string>().empty());
    EXPECT_FALSE(report["fleet_metrics"]["fastest_agent"].get<std::string>().empty());
}

TEST_F(AnalyzerWithDataTest, SaveReport) {
    PerformanceAnalyzer a(tmp_);
    nlohmann::json report = {{"test", "data"}};
    auto path = a.save_report(report, "test_report.json");
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST_F(AnalyzerWithDataTest, SaveReportAutoFilename) {
    PerformanceAnalyzer a(tmp_);
    nlohmann::json report = {{"test", "data"}};
    auto path = a.save_report(report);
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(std::filesystem::exists(path));
    // Auto-generated filename should contain "performance_report_"
    EXPECT_NE(path.find("performance_report_"), std::string::npos);
}

TEST_F(AnalyzerWithDataTest, GetAgentRankings) {
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 100}, {"status", "SUCCESS"}});
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 200}, {"status", "FAILURE"}});
    write_session({{"agent_id", "agent-2"}, {"duration_ms", 50}, {"status", "SUCCESS"}});
    write_session({{"agent_id", "agent-2"}, {"duration_ms", 60}, {"status", "SUCCESS"}});

    PerformanceAnalyzer a(tmp_);
    auto rankings = a.get_agent_rankings(24, "success_rate");
    ASSERT_GE(rankings.size(), 2u);
    // agent-2 has 100% success rate, agent-1 has 50%
    EXPECT_EQ(rankings[0].first, "agent-2");
    EXPECT_NEAR(rankings[0].second, 1.0, 0.01);
    EXPECT_EQ(rankings[1].first, "agent-1");
    EXPECT_NEAR(rankings[1].second, 0.5, 0.01);
}

TEST_F(AnalyzerWithDataTest, GetAgentRankingsEmpty) {
    PerformanceAnalyzer a(tmp_);
    auto rankings = a.get_agent_rankings();
    EXPECT_TRUE(rankings.empty());
}

TEST_F(AnalyzerWithDataTest, AnalyzeDelegationPatternsEmpty) {
    PerformanceAnalyzer a(tmp_);
    auto patterns = a.analyze_delegation_patterns();
    EXPECT_TRUE(patterns.empty());
}

TEST_F(AnalyzerWithDataTest, AnalyzeToolUsagePatternsEmpty) {
    PerformanceAnalyzer a(tmp_);
    auto tools = a.analyze_tool_usage_patterns();
    EXPECT_TRUE(tools.empty());
}

TEST_F(AnalyzerWithDataTest, PercentileTwoValues) {
    std::vector<double> values{10, 20};
    EXPECT_DOUBLE_EQ(PerformanceAnalyzer::calculate_percentile(values, 50), 15.0);
}

TEST_F(AnalyzerWithDataTest, LoadEventsWithCorruptLines) {
    // Write valid events and then corrupt data
    write_event({{"event_type", "test"}, {"properties", {}}});
    std::ofstream f(tmp_ / "events.jsonl", std::ios::app);
    f << "not-valid-json\n";
    f << "\n";  // empty line
    f.close();

    PerformanceAnalyzer a(tmp_);
    // analyze_tool_usage_patterns loads events internally
    auto tools = a.analyze_tool_usage_patterns();
    // Should not crash, corrupt lines are skipped
}

TEST_F(AnalyzerWithDataTest, LoadSessionsWithCorruptLines) {
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 100}, {"status", "SUCCESS"}});
    std::ofstream f(tmp_ / "sessions.jsonl", std::ios::app);
    f << "corrupt-line\n";
    f.close();

    PerformanceAnalyzer a(tmp_);
    auto perf = a.analyze_agent_performance();
    ASSERT_TRUE(perf.contains("agent-1"));
    EXPECT_EQ(perf["agent-1"]["total_tasks"], 1);
}

TEST_F(AnalyzerWithDataTest, WarningStatusTracked) {
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 100}, {"status", "WARNING"}});

    PerformanceAnalyzer a(tmp_);
    auto perf = a.analyze_agent_performance();
    ASSERT_TRUE(perf.contains("agent-1"));
    EXPECT_EQ(perf["agent-1"]["total_tasks"], 1);
    // WARNING is not SUCCESS or FAILURE, so both rates should be 0
    EXPECT_DOUBLE_EQ(perf["agent-1"]["success_rate"].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(perf["agent-1"]["failure_rate"].get<double>(), 0.0);
}

// --- Coverage: lines 16-21 (default_metrics_dir with EUXIS_HOME set) ---
TEST_F(AnalyzerWithDataTest, DefaultMetricsDirFromEuxisHome) {
    auto env_dir = std::filesystem::temp_directory_path() / "euxis_analyzer_env_test";
    std::filesystem::remove_all(env_dir);

    const char* old_euxis = std::getenv("EUXIS_HOME");
    setenv("EUXIS_HOME", env_dir.c_str(), 1);

    // Construct with empty path to trigger default_metrics_dir()
    PerformanceAnalyzer a({});
    auto report = a.generate_performance_report();
    // Should not crash; report generated from empty data
    EXPECT_TRUE(report.contains("report_timestamp"));

    if (old_euxis) setenv("EUXIS_HOME", old_euxis, 1);
    else unsetenv("EUXIS_HOME");
    std::filesystem::remove_all(env_dir);
}

// --- Coverage: lines 19-21 (default_metrics_dir without EUXIS_HOME) ---
TEST_F(AnalyzerWithDataTest, DefaultMetricsDirFromHomeFallback) {
    const char* old_euxis = std::getenv("EUXIS_HOME");
    unsetenv("EUXIS_HOME");

    PerformanceAnalyzer a({});
    auto report = a.generate_performance_report();
    EXPECT_TRUE(report.contains("report_timestamp"));

    if (old_euxis) setenv("EUXIS_HOME", old_euxis, 1);
}

// --- Coverage: lines 44-57 (mean/median helper functions via analysis) ---
TEST_F(AnalyzerWithDataTest, AnalyzeWithEvenNumberOfSessions) {
    // 4 sessions to trigger the even-count median branch (n%2 == 0)
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 100}, {"status", "SUCCESS"}});
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 200}, {"status", "SUCCESS"}});
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 300}, {"status", "SUCCESS"}});
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 400}, {"status", "FAILURE"}});

    PerformanceAnalyzer a(tmp_);
    auto perf = a.analyze_agent_performance();
    ASSERT_TRUE(perf.contains("agent-1"));
    // median of {100, 200, 300, 400} = (200 + 300) / 2 = 250
    EXPECT_NEAR(perf["agent-1"]["median_duration_ms"].get<double>(), 250.0, 0.01);
    EXPECT_NEAR(perf["agent-1"]["avg_duration_ms"].get<double>(), 250.0, 0.01);
}

// --- Coverage: line 93 (load_events empty-line skip) ---
TEST_F(AnalyzerWithDataTest, LoadEventsWithEmptyLines) {
    std::ofstream f(tmp_ / "events.jsonl");
    f << "\n\n";  // Only empty lines
    f << R"({"event_type": "Agent:ToolExecution", "properties": {"tool_name": "test", "execution_duration_ms": 10, "success": true, "retries": 0}})" << "\n";
    f << "\n";
    f.close();

    PerformanceAnalyzer a(tmp_);
    auto tools = a.analyze_tool_usage_patterns();
    ASSERT_TRUE(tools.contains("test"));
    EXPECT_EQ(tools["test"]["total_executions"], 1);
}

// --- Coverage: lines 297-312 (get_agent_rankings with custom metric) ---
TEST_F(AnalyzerWithDataTest, GetAgentRankingsByDuration) {
    write_session({{"agent_id", "slow-agent"}, {"duration_ms", 500}, {"status", "SUCCESS"}});
    write_session({{"agent_id", "fast-agent"}, {"duration_ms", 50}, {"status", "SUCCESS"}});

    PerformanceAnalyzer a(tmp_);
    auto rankings = a.get_agent_rankings(24, "avg_duration_ms");
    ASSERT_EQ(rankings.size(), 2u);
    // Sorted descending by value: slow-agent first (higher avg_duration_ms)
    EXPECT_EQ(rankings[0].first, "slow-agent");
    EXPECT_EQ(rankings[1].first, "fast-agent");
}

// --- Coverage: line 305 (get_agent_rankings with nonexistent metric) ---
TEST_F(AnalyzerWithDataTest, GetAgentRankingsWithNonexistentMetric) {
    write_session({{"agent_id", "agent-1"}, {"duration_ms", 100}, {"status", "SUCCESS"}});

    PerformanceAnalyzer a(tmp_);
    auto rankings = a.get_agent_rankings(24, "nonexistent_metric");
    EXPECT_TRUE(rankings.empty());
}

} // namespace
} // namespace euxis::metrics
