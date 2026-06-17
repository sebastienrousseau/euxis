#include <gtest/gtest.h>
#include <sodium.h>

#include "euxis/bench/runner.hpp"

namespace euxis::bench {

// Forward declarations for all benchmark registration functions
void register_autonomy_benchmarks(BenchmarkRunner& runner);
void register_interop_benchmarks(BenchmarkRunner& runner);
void register_performance_benchmarks(BenchmarkRunner& runner);
void register_portability_benchmarks(BenchmarkRunner& runner);
void register_security_benchmarks(BenchmarkRunner& runner);

namespace {

class BenchSuiteTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";
    }
};

// ---------------------------------------------------------------------------
// Autonomy benchmarks
// ---------------------------------------------------------------------------
TEST_F(BenchSuiteTest, AutonomyRegistration) {
    BenchmarkRunner runner;
    register_autonomy_benchmarks(runner);

    auto suites = runner.suites();
    ASSERT_EQ(suites.size(), 1u);
    EXPECT_EQ(suites[0], "autonomy");
}

TEST_F(BenchSuiteTest, AutonomyTaskLifecycle) {
    BenchmarkRunner runner;
    register_autonomy_benchmarks(runner);

    auto report = runner.run_suite("autonomy");
    EXPECT_EQ(report.suite_name, "autonomy");
    EXPECT_GE(report.results.size(), 2u);

    // Find task_lifecycle result
    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "task_lifecycle") {
            found = true;
            EXPECT_EQ(r.suite, "autonomy");
            EXPECT_EQ(r.unit, "ops/sec");
            EXPECT_GT(r.value, 0.0);
            EXPECT_FALSE(r.message.empty());
        }
    }
    EXPECT_TRUE(found) << "task_lifecycle benchmark not found";
}

TEST_F(BenchSuiteTest, AutonomyErrorRecovery) {
    BenchmarkRunner runner;
    register_autonomy_benchmarks(runner);

    auto report = runner.run_suite("autonomy");

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "error_recovery") {
            found = true;
            EXPECT_EQ(r.suite, "autonomy");
            EXPECT_EQ(r.unit, "%");
            EXPECT_GT(r.value, 0.0);
        }
    }
    EXPECT_TRUE(found) << "error_recovery benchmark not found";
}

// ---------------------------------------------------------------------------
// Interop benchmarks
// ---------------------------------------------------------------------------
TEST_F(BenchSuiteTest, InteropRegistration) {
    BenchmarkRunner runner;
    register_interop_benchmarks(runner);

    auto suites = runner.suites();
    ASSERT_EQ(suites.size(), 1u);
    EXPECT_EQ(suites[0], "interop");
}

TEST_F(BenchSuiteTest, InteropSkillImportThroughput) {
    BenchmarkRunner runner;
    register_interop_benchmarks(runner);

    auto report = runner.run_suite("interop");
    EXPECT_GE(report.results.size(), 2u);

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "skill_import_throughput") {
            found = true;
            EXPECT_EQ(r.suite, "interop");
            EXPECT_EQ(r.unit, "ops/sec");
            EXPECT_GT(r.value, 0.0);
        }
    }
    EXPECT_TRUE(found) << "skill_import_throughput benchmark not found";
}

TEST_F(BenchSuiteTest, InteropA2ACardGeneration) {
    BenchmarkRunner runner;
    register_interop_benchmarks(runner);

    auto report = runner.run_suite("interop");

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "a2a_card_generation") {
            found = true;
            EXPECT_EQ(r.suite, "interop");
            EXPECT_EQ(r.unit, "ms/op");
            EXPECT_GE(r.value, 0.0);
        }
    }
    EXPECT_TRUE(found) << "a2a_card_generation benchmark not found";
}

// ---------------------------------------------------------------------------
// Performance benchmarks
// ---------------------------------------------------------------------------
TEST_F(BenchSuiteTest, PerformanceRegistration) {
    BenchmarkRunner runner;
    register_performance_benchmarks(runner);

    auto suites = runner.suites();
    ASSERT_EQ(suites.size(), 1u);
    EXPECT_EQ(suites[0], "performance");
}

TEST_F(BenchSuiteTest, PerformanceCryptoThroughput) {
    BenchmarkRunner runner;
    register_performance_benchmarks(runner);

    auto report = runner.run_suite("performance");
    EXPECT_GE(report.results.size(), 4u);

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "crypto_throughput") {
            found = true;
            EXPECT_EQ(r.suite, "performance");
            EXPECT_EQ(r.unit, "ops/sec");
            EXPECT_GT(r.value, 0.0);
        }
    }
    EXPECT_TRUE(found) << "crypto_throughput benchmark not found";
}

TEST_F(BenchSuiteTest, PerformanceCryptoThroughputCached) {
    BenchmarkRunner runner;
    register_performance_benchmarks(runner);

    auto report = runner.run_suite("performance");

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "crypto_throughput_cached") {
            found = true;
            EXPECT_EQ(r.suite, "performance");
            EXPECT_EQ(r.unit, "ops/sec");
            EXPECT_GT(r.value, 0.0);
        }
    }
    EXPECT_TRUE(found) << "crypto_throughput_cached benchmark not found";
}

TEST_F(BenchSuiteTest, PerformanceKeyDerivationP95) {
    BenchmarkRunner runner;
    register_performance_benchmarks(runner);

    auto report = runner.run_suite("performance");

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "key_derivation_p95") {
            found = true;
            EXPECT_EQ(r.suite, "performance");
            EXPECT_EQ(r.unit, "ms");
            EXPECT_GT(r.value, 0.0);
        }
    }
    EXPECT_TRUE(found) << "key_derivation_p95 benchmark not found";
}

TEST_F(BenchSuiteTest, PerformanceSerializationSpeed) {
    BenchmarkRunner runner;
    register_performance_benchmarks(runner);

    auto report = runner.run_suite("performance");

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "serialization_speed") {
            found = true;
            EXPECT_EQ(r.suite, "performance");
            EXPECT_EQ(r.unit, "us/op");
            EXPECT_GE(r.value, 0.0);
        }
    }
    EXPECT_TRUE(found) << "serialization_speed benchmark not found";
}

// ---------------------------------------------------------------------------
// Portability benchmarks
// ---------------------------------------------------------------------------
TEST_F(BenchSuiteTest, PortabilityRegistration) {
    BenchmarkRunner runner;
    register_portability_benchmarks(runner);

    auto suites = runner.suites();
    ASSERT_EQ(suites.size(), 1u);
    EXPECT_EQ(suites[0], "portability");
}

TEST_F(BenchSuiteTest, PortabilityPlatformDetection) {
    BenchmarkRunner runner;
    register_portability_benchmarks(runner);

    auto report = runner.run_suite("portability");
    EXPECT_GE(report.results.size(), 2u);

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "platform_detection") {
            found = true;
            EXPECT_EQ(r.suite, "portability");
            EXPECT_EQ(r.unit, "%");
            EXPECT_EQ(r.value, 100.0);
            EXPECT_TRUE(r.passed);
        }
    }
    EXPECT_TRUE(found) << "platform_detection benchmark not found";
}

TEST_F(BenchSuiteTest, PortabilityPathNormalization) {
    BenchmarkRunner runner;
    register_portability_benchmarks(runner);

    auto report = runner.run_suite("portability");

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "path_normalization") {
            found = true;
            EXPECT_EQ(r.suite, "portability");
            EXPECT_EQ(r.unit, "ops/sec");
            EXPECT_GT(r.value, 0.0);
        }
    }
    EXPECT_TRUE(found) << "path_normalization benchmark not found";
}

// ---------------------------------------------------------------------------
// Security benchmarks
// ---------------------------------------------------------------------------
TEST_F(BenchSuiteTest, SecurityRegistration) {
    BenchmarkRunner runner;
    register_security_benchmarks(runner);

    auto suites = runner.suites();
    ASSERT_EQ(suites.size(), 1u);
    EXPECT_EQ(suites[0], "security");
}

TEST_F(BenchSuiteTest, SecurityMaliciousDetectionRate) {
    BenchmarkRunner runner;
    register_security_benchmarks(runner);

    auto report = runner.run_suite("security");
    EXPECT_GE(report.results.size(), 2u);

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "malicious_detection_rate") {
            found = true;
            EXPECT_EQ(r.suite, "security");
            EXPECT_EQ(r.unit, "%");
            EXPECT_GT(r.value, 0.0);
        }
    }
    EXPECT_TRUE(found) << "malicious_detection_rate benchmark not found";
}

TEST_F(BenchSuiteTest, SecuritySandboxEscapeResistance) {
    BenchmarkRunner runner;
    register_security_benchmarks(runner);

    auto report = runner.run_suite("security");

    bool found = false;
    for (const auto& r : report.results) {
        if (r.name == "sandbox_escape_resistance") {
            found = true;
            EXPECT_EQ(r.suite, "security");
            EXPECT_EQ(r.unit, "%");
            EXPECT_EQ(r.value, 100.0);
            EXPECT_TRUE(r.passed);
        }
    }
    EXPECT_TRUE(found) << "sandbox_escape_resistance benchmark not found";
}

// ---------------------------------------------------------------------------
// register_defaults covers all suites
// ---------------------------------------------------------------------------
TEST_F(BenchSuiteTest, RegisterDefaultsAll) {
    BenchmarkRunner runner;
    runner.register_defaults();

    auto suites = runner.suites();
    EXPECT_GE(suites.size(), 5u);

    // Verify all expected suites are present
    auto contains = [&](const std::string& name) {
        return std::find(suites.begin(), suites.end(), name) != suites.end();
    };
    EXPECT_TRUE(contains("security"));
    EXPECT_TRUE(contains("autonomy"));
    EXPECT_TRUE(contains("performance"));
    EXPECT_TRUE(contains("portability"));
    EXPECT_TRUE(contains("interop"));
}

TEST_F(BenchSuiteTest, RegisterDefaultsRunAll) {
    BenchmarkRunner runner;
    runner.register_defaults();

    auto reports = runner.run_all();
    EXPECT_GE(reports.size(), 5u);

    // Every suite should have at least one result
    for (const auto& report : reports) {
        EXPECT_FALSE(report.results.empty())
            << "Suite " << report.suite_name << " has no results";
    }
}

// ---------------------------------------------------------------------------
// Verify benchmark result JSON serialization
// ---------------------------------------------------------------------------
TEST_F(BenchSuiteTest, BenchmarkResultsSerialize) {
    BenchmarkRunner runner;
    register_security_benchmarks(runner);

    auto report = runner.run_suite("security");
    auto j = report.to_json();

    EXPECT_EQ(j["suite_name"], "security");
    EXPECT_TRUE(j.contains("results"));
    EXPECT_TRUE(j["results"].is_array());
    EXPECT_GE(j["results"].size(), 2u);

    for (const auto& result_json : j["results"]) {
        EXPECT_TRUE(result_json.contains("name"));
        EXPECT_TRUE(result_json.contains("suite"));
        EXPECT_TRUE(result_json.contains("passed"));
        EXPECT_TRUE(result_json.contains("value"));
        EXPECT_TRUE(result_json.contains("unit"));
        EXPECT_TRUE(result_json.contains("target"));
        EXPECT_TRUE(result_json.contains("duration_us"));
        EXPECT_TRUE(result_json.contains("message"));
    }
}

} // namespace
} // namespace euxis::bench
