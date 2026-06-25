#include "euxis/cli/playbook_verify.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace euxis::cli::cmd {

namespace {

PlaybookEvidence ev(std::string name, std::string verdict, std::string raw = "", double dur = 100.0) {
    return PlaybookEvidence{name, name, std::move(verdict), std::move(raw), dur};
}

bool has_rule(const std::vector<VerdictViolation>& vs, const std::string& rule) {
    return std::any_of(vs.begin(), vs.end(), [&](const VerdictViolation& v) { return v.rule == rule; });
}

class DriftFixture : public ::testing::Test {
protected:
    std::filesystem::path home_;
    std::filesystem::path history_path_;

    void SetUp() override {
        home_ = std::filesystem::temp_directory_path() / ("euxis-pbverify-" + std::to_string(::getpid()) + "-" + test_id());
        std::filesystem::remove_all(home_);
        std::filesystem::create_directories(home_ / "data" / "runtime" / "sessions");
        history_path_ = home_ / "data" / "runtime" / "sessions" / "history.jsonl";
    }
    void TearDown() override {
        std::filesystem::remove_all(home_);
    }

    // Append one history entry: agent_name -> {duration_ms, status}.
    void append_history(const std::vector<std::tuple<std::string, double, std::string>>& agents) {
        nlohmann::json entry;
        entry["agent_status"] = nlohmann::json::object();
        for (const auto& [name, dur, status] : agents) {
            entry["agent_status"][name] = {{"duration_ms", dur}, {"status", status}};
        }
        std::ofstream f(history_path_, std::ios::app);
        f << entry.dump() << "\n";
    }

private:
    static std::string test_id() {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        return std::string(info->test_suite_name()) + "-" + info->name();
    }
};

} // namespace

// ----- R1: TRUSTED constraints -----

TEST(VerifyVerdictConsistency, TrustedWithContradictionViolatesR1) {
    auto vs = verify_verdict_consistency("TRUSTED", {ev("a", "PASS")}, 90, /*contradictions=*/1);
    EXPECT_TRUE(has_rule(vs, "R1"));
}

TEST(VerifyVerdictConsistency, TrustedWithFailViolatesR1) {
    auto vs = verify_verdict_consistency("TRUSTED",
        {ev("a", "PASS"), ev("b", "FAILED")}, 90, 0);
    EXPECT_TRUE(has_rule(vs, "R1"));
}

TEST(VerifyVerdictConsistency, TrustedWithTimeoutViolatesR1) {
    auto vs = verify_verdict_consistency("TRUSTED",
        {ev("a", "PASS"), ev("b", "TIMEOUT")}, 90, 0);
    EXPECT_TRUE(has_rule(vs, "R1"));
}

TEST(VerifyVerdictConsistency, TrustedWithLowConfidenceViolatesR1) {
    auto vs = verify_verdict_consistency("TRUSTED", {ev("a", "PASS")}, 60, 0);
    EXPECT_TRUE(has_rule(vs, "R1"));
}

TEST(VerifyVerdictConsistency, TrustedCleanPassesAllRules) {
    auto vs = verify_verdict_consistency("TRUSTED",
        {ev("a", "PASS"), ev("b", "PASS")}, 90, 0);
    EXPECT_TRUE(vs.empty());
}

// ----- R2: BLOCKED constraints -----

TEST(VerifyVerdictConsistency, BlockedWithNoFailsViolatesR2) {
    auto vs = verify_verdict_consistency("BLOCKED", {ev("a", "PARTIAL")}, 50, 0);
    EXPECT_TRUE(has_rule(vs, "R2"));
}

TEST(VerifyVerdictConsistency, BlockedWithPassesViolatesR2) {
    auto vs = verify_verdict_consistency("BLOCKED",
        {ev("a", "FAILED"), ev("b", "PASS")}, 50, 0);
    EXPECT_TRUE(has_rule(vs, "R2"));
}

TEST(VerifyVerdictConsistency, BlockedUnanimousFailsPassesR2) {
    auto vs = verify_verdict_consistency("BLOCKED",
        {ev("a", "FAILED"), ev("b", "FAILED")}, 50, 0);
    EXPECT_FALSE(has_rule(vs, "R2"));
}

// ----- R3: INCONCLUSIVE constraints -----

TEST(VerifyVerdictConsistency, InconclusiveWithoutContradictionsViolatesR3) {
    auto vs = verify_verdict_consistency("INCONCLUSIVE", {ev("a", "PASS")}, 50, 0);
    EXPECT_TRUE(has_rule(vs, "R3"));
}

TEST(VerifyVerdictConsistency, InconclusiveWithContradictionsPassesR3) {
    auto vs = verify_verdict_consistency("INCONCLUSIVE", {ev("a", "PASS")}, 50, 1);
    EXPECT_FALSE(has_rule(vs, "R3"));
}

// ----- R4: All-TIMEOUT trusted is invalid -----

TEST(VerifyVerdictConsistency, TrustedAllTimeoutViolatesR4) {
    auto vs = verify_verdict_consistency("TRUSTED",
        {ev("a", "TIMEOUT"), ev("b", "TIMEOUT")}, 90, 0);
    EXPECT_TRUE(has_rule(vs, "R4"));
}

TEST(VerifyVerdictConsistency, TrustedEmptyEvidenceSkipsR4) {
    auto vs = verify_verdict_consistency("TRUSTED", {}, 90, 0);
    EXPECT_FALSE(has_rule(vs, "R4"));
}

// ----- R5: Majority decisive negatives -----

TEST(VerifyVerdictConsistency, TrustedMajorityFailsViolatesR5) {
    auto vs = verify_verdict_consistency("TRUSTED",
        {ev("a", "FAILED"), ev("b", "FAILED"), ev("c", "FAILED"), ev("d", "PASS"), ev("e", "PASS")},
        80, 0);
    EXPECT_TRUE(has_rule(vs, "R5"));
}

TEST(VerifyVerdictConsistency, TrustedWithGapsMajorityFailsViolatesR5) {
    auto vs = verify_verdict_consistency("TRUSTED WITH GAPS",
        {ev("a", "FAILED"), ev("b", "FAILED"), ev("c", "FAILED"), ev("d", "PASS")},
        70, 0);
    EXPECT_TRUE(has_rule(vs, "R5"));
}

TEST(VerifyVerdictConsistency, EffectiveFailIncludesTimeoutFail) {
    // TIMEOUT with raw_verdict=FAIL counts toward effective_fails for R1/R5.
    auto vs = verify_verdict_consistency("TRUSTED",
        {ev("a", "TIMEOUT", "FAIL")}, 90, 0);
    EXPECT_TRUE(has_rule(vs, "R1"));
}

// ----- Drift detector -----

TEST_F(DriftFixture, ReturnsEmptyWhenHistoryAbsent) {
    auto alerts = detect_agent_drift(home_.string(), {ev("alpha", "PASS", "", 100.0)});
    EXPECT_TRUE(alerts.empty());
}

TEST_F(DriftFixture, ReturnsEmptyWhenHistoryTooSmall) {
    append_history({{"alpha", 100, "PASS"}});
    append_history({{"alpha", 110, "PASS"}});
    auto alerts = detect_agent_drift(home_.string(), {ev("alpha", "PASS", "", 100.0)});
    EXPECT_TRUE(alerts.empty());
}

TEST_F(DriftFixture, LatencySpikeBeyondTwoSigmaTriggersAlert) {
    // Need some variance in baseline so stddev > 0.
    for (double v : {95.0, 100.0, 105.0, 98.0, 102.0}) {
        append_history({{"alpha", v, "PASS"}});
    }
    // current 1000ms → way beyond mean±2σ (mean≈100, σ≈3.5)
    auto alerts = detect_agent_drift(home_.string(), {ev("alpha", "PASS", "", 1000.0)});
    ASSERT_FALSE(alerts.empty());
    bool latency_alert = false;
    for (const auto& a : alerts) {
        if (a.metric == "latency") {
            latency_alert = true;
            EXPECT_EQ(a.severity, "alert");
            EXPECT_GT(a.deviation_pct, 100.0);
        }
    }
    EXPECT_TRUE(latency_alert);
}

TEST_F(DriftFixture, LatencySpikeUnderHundredIsWarning) {
    // Baseline 100ms ± 5; current 220ms — > 2σ but only ~120% deviation.
    // Need deviation between 0 and 100 to land on warning, so push baseline higher.
    for (int i = 0; i < 5; ++i) append_history({{"alpha", 1000.0 + i, "PASS"}});
    // current 1500ms → deviation ~50%
    auto alerts = detect_agent_drift(home_.string(), {ev("alpha", "PASS", "", 1500.0)});
    ASSERT_FALSE(alerts.empty());
    bool warning = false;
    for (const auto& a : alerts) {
        if (a.metric == "latency") {
            EXPECT_EQ(a.severity, "warning");
            warning = true;
        }
    }
    EXPECT_TRUE(warning);
}

TEST_F(DriftFixture, TimeoutSpikeFromCleanHistoryTriggersDistributionAlert) {
    // 5 clean PASS entries → hist_timeout_rate = 0 < 0.3; current TIMEOUT.
    for (int i = 0; i < 5; ++i) append_history({{"alpha", 100, "PASS"}});
    auto alerts = detect_agent_drift(home_.string(), {ev("alpha", "TIMEOUT", "", 100.0)});
    bool distribution = false;
    for (const auto& a : alerts) {
        if (a.metric == "verdict_distribution") {
            distribution = true;
            EXPECT_EQ(a.severity, "warning");
        }
    }
    EXPECT_TRUE(distribution);
}

TEST_F(DriftFixture, NoAlertWhenHistoryAlreadyShowsTimeouts) {
    // 5 TIMEOUTs in history → rate 1.0 >= 0.3; current TIMEOUT not a drift.
    for (int i = 0; i < 5; ++i) append_history({{"alpha", 100, "TIMEOUT"}});
    auto alerts = detect_agent_drift(home_.string(), {ev("alpha", "TIMEOUT", "", 100.0)});
    bool distribution = false;
    for (const auto& a : alerts) {
        if (a.metric == "verdict_distribution") distribution = true;
    }
    EXPECT_FALSE(distribution);
}

TEST_F(DriftFixture, TrimsHistoryToLastTen) {
    // 15 entries with the early ones at 100ms and last 5 at 1000ms.
    for (int i = 0; i < 5; ++i) append_history({{"alpha", 100, "PASS"}});
    for (int i = 0; i < 10; ++i) append_history({{"alpha", 1000, "PASS"}});
    // detect_agent_drift takes last 10, so baseline ≈ 1000ms; current 1000ms → no alert.
    auto alerts = detect_agent_drift(home_.string(), {ev("alpha", "PASS", "", 1000.0)});
    bool latency = false;
    for (const auto& a : alerts) {
        if (a.metric == "latency") latency = true;
    }
    EXPECT_FALSE(latency);
}

TEST_F(DriftFixture, IgnoresAgentsWithoutHistoryEntries) {
    for (int i = 0; i < 5; ++i) append_history({{"alpha", 100, "PASS"}});
    // Current evidence references "beta" which never appears in history.
    auto alerts = detect_agent_drift(home_.string(), {ev("beta", "TIMEOUT", "", 9999.0)});
    EXPECT_TRUE(alerts.empty());
}

TEST_F(DriftFixture, SkipsMalformedHistoryLines) {
    {
        std::ofstream f(history_path_);
        f << "not json\n";
        // Valid entries with variance so stddev > 0.
        for (double v : {95.0, 100.0, 105.0, 98.0, 102.0}) {
            nlohmann::json e;
            e["agent_status"]["alpha"] = {{"duration_ms", v}, {"status", "PASS"}};
            f << e.dump() << "\n";
        }
        f << "{also broken\n";
    }
    auto alerts = detect_agent_drift(home_.string(), {ev("alpha", "PASS", "", 1000.0)});
    bool latency = false;
    for (const auto& a : alerts) {
        if (a.metric == "latency") latency = true;
    }
    EXPECT_TRUE(latency);
}

} // namespace euxis::cli::cmd
