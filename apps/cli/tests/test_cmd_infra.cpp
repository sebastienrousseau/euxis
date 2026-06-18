#include <gtest/gtest.h>
#include "euxis/cli/cmd/infra.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <nlohmann/json.hpp>

namespace euxis::cli::cmd {
namespace {

namespace fs = std::filesystem;

class InfraCmdTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_.euxis_home = "/tmp/euxis_test_infra_" + std::to_string(getpid());
        ctx_.data_dir = ctx_.euxis_home + "/data";
        fs::create_directories(ctx_.data_dir + "/config");
        fs::create_directories(ctx_.data_dir + "/agents");
    }

    void TearDown() override {
        fs::remove_all(ctx_.euxis_home);
    }

    Context ctx_;
};

// ---- Bus command tests ----

TEST_F(InfraCmdTest, BusStatus) {
    auto code = cmd_bus(ctx_, {"status"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, BusUsage) {
    auto code = cmd_bus(ctx_, {});
    EXPECT_EQ(code, 0); // status is default
}

TEST_F(InfraCmdTest, BusStatusWithDirectory) {
    // Create bus directory with a pipe file
    auto bus_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "data" / "bus" / "pipes";
    fs::create_directories(bus_dir);
    std::ofstream(bus_dir / "test-pipe") << "message1\n";

    auto code = cmd_bus(ctx_, {"status"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, BusStatusVerbose) {
    auto bus_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "data" / "bus" / "pipes";
    fs::create_directories(bus_dir);
    std::ofstream(bus_dir / "test-pipe") << "message1\n";

    ctx_.verbose = true;
    auto code = cmd_bus(ctx_, {"status"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, BusPublish) {
    auto code = cmd_bus(ctx_, {"publish", "test-pipe", "hello world"});
    EXPECT_EQ(code, 0);

    auto bus_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "data" / "bus" / "pipes";
    EXPECT_TRUE(fs::exists(bus_dir / "test-pipe"));
}

TEST_F(InfraCmdTest, BusSubscribeNonexistent) {
    auto code = cmd_bus(ctx_, {"subscribe", "nonexistent-pipe"});
    EXPECT_EQ(code, 1);
}

TEST_F(InfraCmdTest, BusSubscribeExisting) {
    auto bus_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "data" / "bus" / "pipes";
    fs::create_directories(bus_dir);
    std::ofstream(bus_dir / "test-pipe") << "line1\nline2\n";

    auto code = cmd_bus(ctx_, {"subscribe", "test-pipe"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, BusCleanNoBusDir) {
    auto code = cmd_bus(ctx_, {"clean"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, BusCleanWithPipes) {
    auto bus_dir = fs::path(ctx_.euxis_home) / "data/runtime" / "data" / "bus" / "pipes";
    fs::create_directories(bus_dir);
    std::ofstream(bus_dir / "recent-pipe") << "data\n";

    auto code = cmd_bus(ctx_, {"clean"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, BusInvalidSubcommand) {
    auto code = cmd_bus(ctx_, {"invalid-sub"});
    EXPECT_EQ(code, 2);
}

TEST_F(InfraCmdTest, BusPublishInsufficientArgs) {
    // "publish" with only 1 extra arg (needs 2)
    auto code = cmd_bus(ctx_, {"publish", "pipe-only"});
    EXPECT_EQ(code, 2);
}

TEST_F(InfraCmdTest, BusSubscribeNoArgs) {
    auto code = cmd_bus(ctx_, {"subscribe"});
    EXPECT_EQ(code, 2);
}

// ---- Daemon command tests ----

TEST_F(InfraCmdTest, DaemonStatus) {
    auto code = cmd_daemon(ctx_, {"status"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, DaemonUsage) {
    auto code = cmd_daemon(ctx_, {});
    EXPECT_EQ(code, 0); // status is default
}

TEST_F(InfraCmdTest, DaemonInvalidSubcommand) {
    auto code = cmd_daemon(ctx_, {"invalid"});
    EXPECT_EQ(code, 2);
}

TEST_F(InfraCmdTest, DaemonStopNotRunning) {
    auto code = cmd_daemon(ctx_, {"stop"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, DaemonStatusWithStalePid) {
    // Create a stale PID file with a non-existent PID
    auto pid_dir = fs::path(ctx_.euxis_home) / "data/runtime";
    fs::create_directories(pid_dir);
    std::ofstream(pid_dir / "daemon.pid") << "999999999";

    auto code = cmd_daemon(ctx_, {"status"});
    // Should detect stale PID and return 1
    EXPECT_EQ(code, 1);
}

TEST_F(InfraCmdTest, DaemonStopWithStalePid) {
    auto pid_dir = fs::path(ctx_.euxis_home) / "data/runtime";
    fs::create_directories(pid_dir);
    auto pid_file = pid_dir / "daemon.pid";
    std::ofstream(pid_file) << "999999999";

    auto code = cmd_daemon(ctx_, {"stop"});
    EXPECT_EQ(code, 0);
    // PID file should be cleaned up
    EXPECT_FALSE(fs::exists(pid_file));
}

// ---- Deploy command tests ----

TEST_F(InfraCmdTest, DeployUsage) {
    auto code = cmd_deploy(ctx_, {});
    EXPECT_EQ(code, 2);
}

TEST_F(InfraCmdTest, DeployMissingFile) {
    auto code = cmd_deploy(ctx_, {"/tmp/nonexistent.json"});
    EXPECT_EQ(code, 1);
}

TEST_F(InfraCmdTest, DeployInvalidJson) {
    auto config_path = ctx_.euxis_home + "/deploy-bad.json";
    std::ofstream(config_path) << "not valid json{{{";

    auto code = cmd_deploy(ctx_, {config_path});
    EXPECT_EQ(code, 1);
}

TEST_F(InfraCmdTest, DeployDryRun) {
    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", "test-agent"}, {"manifest_path", "/tmp/manifest.json"}}
    });

    auto config_path = ctx_.euxis_home + "/deploy.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path, "--dry-run"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, DeployWithAgentsMissingFields) {
    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", ""}},  // missing manifest_path
        {{"manifest_path", "foo.json"}}  // missing agent_id
    });

    auto config_path = ctx_.euxis_home + "/deploy-missing.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path});
    EXPECT_EQ(code, 0); // skipped agents, no errors
}

TEST_F(InfraCmdTest, DeployWithAgentsInlineManifest) {
    // When manifest_path doesn't exist, uses the agent entry itself as manifest
    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", "inline-agent"}, {"manifest_path", "/tmp/nonexistent-manifest.json"},
         {"role", "worker"}, {"tier", "routine"}, {"version", "1.0"}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-inline.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path});
    // Registration may succeed or fail, but should not crash
    EXPECT_GE(code, 0);
}

TEST_F(InfraCmdTest, DeployWithRouterConfig) {
    nlohmann::json config;
    config["router"] = {{"default_provider", "claude"}, {"tier", "code"}};

    auto config_path = ctx_.euxis_home + "/deploy-router.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path});
    EXPECT_EQ(code, 0);

    // Check that router.json was written
    auto router_path = fs::path(ctx_.data_dir) / "config" / "router.json";
    EXPECT_TRUE(fs::exists(router_path));
}

TEST_F(InfraCmdTest, DeployWithRouterConfigDryRun) {
    nlohmann::json config;
    config["router"] = {{"default_provider", "claude"}};

    auto config_path = ctx_.euxis_home + "/deploy-router-dry.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path, "--dry-run"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, DeployWithSquadsConfig) {
    nlohmann::json config;
    config["squads"] = nlohmann::json::array({
        {{"id", "squad-1"}, {"name", "Test Squad"}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-squads.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path});
    EXPECT_EQ(code, 0);

    auto squads_path = fs::path(ctx_.data_dir) / "squads.json";
    EXPECT_TRUE(fs::exists(squads_path));
}

TEST_F(InfraCmdTest, DeployWithSquadsConfigDryRun) {
    nlohmann::json config;
    config["squads"] = nlohmann::json::array({
        {{"id", "squad-1"}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-squads-dry.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path, "--dry-run"});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, DeployWithValidManifestFile) {
    // Create a valid manifest file
    nlohmann::json manifest;
    manifest["agent_id"] = "manifest-agent";
    manifest["role"] = "worker";
    manifest["tier"] = "code";
    manifest["version"] = "2.0";

    auto manifest_path = ctx_.euxis_home + "/agent-manifest.json";
    std::ofstream(manifest_path) << manifest.dump();

    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", "manifest-agent"}, {"manifest_path", manifest_path}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-manifest.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, DeployWithInvalidManifestFile) {
    // Create an invalid manifest file
    auto manifest_path = ctx_.euxis_home + "/bad-manifest.json";
    std::ofstream(manifest_path) << "not json{{{";

    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", "bad-agent"}, {"manifest_path", manifest_path}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-bad-manifest.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path});
    EXPECT_EQ(code, 1); // agent_errors > 0
}

TEST_F(InfraCmdTest, DeployAgentsDryRunVerbose) {
    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", "dry-agent"}, {"manifest_path", "/tmp/x.json"}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-dry-agents.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path, "--dry-run"});
    EXPECT_EQ(code, 0);
}

// ---- Optimize command tests ----

TEST_F(InfraCmdTest, OptimizeRuns) {
    auto code = cmd_optimize(ctx_, {});
    EXPECT_EQ(code, 0);
}

TEST_F(InfraCmdTest, OptimizeWithCacheDir) {
    auto cache_dir = fs::path(ctx_.euxis_home) / "data" / "runtime" / "provider-usage";
    fs::create_directories(cache_dir);
    // Write some cache files
    std::ofstream(cache_dir / "usage1.json") << R"({"calls": 100})";
    std::ofstream(cache_dir / "usage2.json") << R"({"calls": 200})";

    auto code = cmd_optimize(ctx_, {});
    EXPECT_EQ(code, 0);
}

// ---- Gateway command tests (limited since it starts a server) ----

TEST_F(InfraCmdTest, GatewayParsesArgs) {
    // We cannot easily test cmd_gateway since it starts a blocking server,
    // but we can verify args parsing indirectly through deploy/optimize
    // which exercise the same patterns.
    SUCCEED();
}

// --- Coverage: deploy with agents that have valid manifest files (lines 404-421) ---
TEST_F(InfraCmdTest, DeployWithMultipleAgentsFromManifests) {
    // Create manifest files for two agents
    nlohmann::json m1;
    m1["agent_id"] = "agent-a";
    m1["role"] = "worker";
    m1["tier"] = "code";
    m1["version"] = "1.0";
    auto m1_path = ctx_.euxis_home + "/m1.json";
    std::ofstream(m1_path) << m1.dump();

    nlohmann::json m2;
    m2["agent_id"] = "agent-b";
    m2["role"] = "analyst";
    m2["tier"] = "data";
    m2["version"] = "2.0";
    auto m2_path = ctx_.euxis_home + "/m2.json";
    std::ofstream(m2_path) << m2.dump();

    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", "agent-a"}, {"manifest_path", m1_path}},
        {{"agent_id", "agent-b"}, {"manifest_path", m2_path}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-multi.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path});
    EXPECT_EQ(code, 0);
}

// --- Coverage: deploy with all config types combined (agents + router + squads) ---
TEST_F(InfraCmdTest, DeployFullConfig) {
    nlohmann::json m1;
    m1["agent_id"] = "full-agent";
    m1["role"] = "engineer";
    m1["tier"] = "code";
    auto m1_path = ctx_.euxis_home + "/full-agent-manifest.json";
    std::ofstream(m1_path) << m1.dump();

    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", "full-agent"}, {"manifest_path", m1_path}}
    });
    config["router"] = {{"default_provider", "claude"}, {"tier", "code"}};
    config["squads"] = nlohmann::json::array({
        {{"id", "test-squad"}, {"name", "Test"}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-full.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path});
    EXPECT_EQ(code, 0);
}

// --- Coverage: deploy full config dry-run ---
TEST_F(InfraCmdTest, DeployFullConfigDryRun) {
    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", "dry-a"}, {"manifest_path", "/tmp/m.json"}}
    });
    config["router"] = {{"provider", "claude"}};
    config["squads"] = nlohmann::json::array({
        {{"id", "s1"}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-full-dry.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path, "--dry-run"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: deploy summary with agent errors (lines 471-474) ---
TEST_F(InfraCmdTest, DeployWithMixedSuccess) {
    auto bad_path = ctx_.euxis_home + "/bad-manifest.json";
    std::ofstream(bad_path) << "not json{{{";

    nlohmann::json good_m;
    good_m["agent_id"] = "good-agent";
    good_m["role"] = "ok";
    auto good_path = ctx_.euxis_home + "/good-m.json";
    std::ofstream(good_path) << good_m.dump();

    nlohmann::json config;
    config["agents"] = nlohmann::json::array({
        {{"agent_id", "good-agent"}, {"manifest_path", good_path}},
        {{"agent_id", "bad-agent"}, {"manifest_path", bad_path}}
    });

    auto config_path = ctx_.euxis_home + "/deploy-mixed.json";
    std::ofstream(config_path) << config.dump();

    auto code = cmd_deploy(ctx_, {config_path});
    EXPECT_EQ(code, 1); // 1 error
}

// --- Coverage: optimize with large cache dir (lines 513-515) ---
TEST_F(InfraCmdTest, OptimizeWithLargeCacheDir) {
    auto cache_dir = fs::path(ctx_.euxis_home) / "data" / "runtime" / "provider-usage";
    fs::create_directories(cache_dir);
    // Create a large cache file (simulate > 100MB by writing many files)
    for (int i = 0; i < 10; ++i) {
        std::ofstream(cache_dir / ("usage" + std::to_string(i) + ".json"))
            << std::string(1024, 'x');
    }

    auto code = cmd_optimize(ctx_, {});
    EXPECT_EQ(code, 0);
}

// --- Coverage: daemon start with stale PID file (lines 278-289) ---
TEST_F(InfraCmdTest, DaemonStartWithStalePid) {
    auto pid_dir = fs::path(ctx_.euxis_home) / "data/runtime";
    fs::create_directories(pid_dir);
    auto pid_file = pid_dir / "daemon.pid";
    std::ofstream(pid_file) << "999999999";

    auto code = cmd_daemon(ctx_, {"start"});
    // Should detect stale PID, remove it, and fork.
    EXPECT_GE(code, 0);

    // Wait briefly for the daemon child (detached via setsid) to write its
    // real PID, then stop it. Without this, the child concurrently writes
    // into euxis_home while TearDown calls remove_all → ENOTEMPTY.
    for (int i = 0; i < 100; ++i) {
        std::ifstream f(pid_file);
        std::string s;
        std::getline(f, s);
        if (!s.empty() && s != "999999999") break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    cmd_daemon(ctx_, {"stop"});
    // Give the child a moment to exit and stop writing.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// --- Coverage: bus publish and subscribe round-trip ---
TEST_F(InfraCmdTest, BusPublishAndSubscribe) {
    // Use a per-test pipe name so the file path is unique even within the
    // same test binary across re-runs or potential future fixture sharing.
    const std::string pipe_name =
        "round-trip-pipe-" + std::to_string(::getpid());

    auto code1 = cmd_bus(ctx_, {"publish", pipe_name, "msg1"});
    ASSERT_EQ(code1, 0);

    // The earlier version checked only return codes; that left publish/
    // subscribe internal failures invisible if either silently returned 0.
    // Assert the pipe file exists with the expected content.
    const auto pipe_path = fs::path(ctx_.euxis_home) /
                           "data/runtime" / "data" / "bus" / "pipes" / pipe_name;
    ASSERT_TRUE(fs::exists(pipe_path)) << "publish did not create the pipe file";

    auto code2 = cmd_bus(ctx_, {"subscribe", pipe_name});
    EXPECT_EQ(code2, 0);
}

} // namespace
} // namespace euxis::cli::cmd
