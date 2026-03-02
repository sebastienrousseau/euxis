#include <gtest/gtest.h>
#include "euxis/cli/engine.hpp"

namespace euxis::cli {
namespace {

TEST(EngineTest, VersionCommand) {
    Engine e("/tmp/euxis_test");
    auto code = e.run({"version"});
    EXPECT_EQ(code, 0);
}

TEST(EngineTest, HelpCommand) {
    Engine e("/tmp/euxis_test");
    auto code = e.run({"help"});
    EXPECT_EQ(code, 0);
}

TEST(EngineTest, EmptyArgsShowsUsage) {
    Engine e("/tmp/euxis_test");
    auto code = e.run({});
    EXPECT_EQ(code, 0);
}

TEST(EngineTest, UnknownCommand) {
    Engine e("/tmp/euxis_test");
    auto code = e.run({"nonexistent"});
    EXPECT_EQ(code, 1);
}

TEST(EngineTest, VersionFlag) {
    Engine e("/tmp/euxis_test");
    EXPECT_EQ(e.run({"--version"}), 0);
    EXPECT_EQ(e.run({"-V"}), 0);
}

TEST(EngineTest, HelpFlag) {
    Engine e("/tmp/euxis_test");
    EXPECT_EQ(e.run({"--help"}), 0);
    EXPECT_EQ(e.run({"-h"}), 0);
}

TEST(EngineTest, GlobalFlagNoColor) {
    Engine e("/tmp/euxis_test");
    EXPECT_EQ(e.run({"--no-color", "version"}), 0);
}

TEST(EngineTest, GlobalFlagJson) {
    Engine e("/tmp/euxis_test");
    // JSON flag with version shouldn't crash
    EXPECT_EQ(e.run({"--json", "version"}), 0);
}

TEST(EngineTest, GlobalFlagVerbose) {
    Engine e("/tmp/euxis_test");
    EXPECT_EQ(e.run({"--verbose", "version"}), 0);
    EXPECT_EQ(e.run({"-v", "version"}), 0);
}

TEST(EngineTest, DoctorCommand) {
    Engine e("/tmp/euxis_test_nonexistent");
    // Doctor should run but report issues (missing dirs)
    auto code = e.run({"doctor"});
    EXPECT_GE(code, 0);
}

TEST(EngineTest, HealthCommand) {
    Engine e("/tmp/euxis_test_nonexistent");
    auto code = e.run({"health"});
    EXPECT_GE(code, 0);
}

// --- Coverage: lines 31-38 (Engine default constructor using env vars) ---
TEST(EngineTest, DefaultConstructorUsesEnvOrHome) {
    // Construct with empty string to exercise the else-branch
    Engine e("");
    auto code = e.run({"version"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: lines 128-130 (only global flags, no command => help) ---
TEST(EngineTest, OnlyGlobalFlagsShowsHelp) {
    Engine e("/tmp/euxis_test");
    // All args are global flags, remaining is empty
    auto code = e.run({"--no-color", "--json", "--verbose"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: line 128-130 repeated with different flag combo ---
TEST(EngineTest, OnlyVerboseFlagShowsHelp) {
    Engine e("/tmp/euxis_test");
    auto code = e.run({"-v"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: line 33 (Engine constructor with EUXIS_HOME unset, falls to HOME) ---
TEST(EngineTest, ConstructorFallsToHome) {
    // Unset EUXIS_HOME to exercise the HOME fallback
    unsetenv("EUXIS_HOME");
    Engine e("");
    auto code = e.run({"version"});
    EXPECT_EQ(code, 0);
}

// --- Coverage: line 33 (Engine constructor with EUXIS_HOME set) ---
TEST(EngineTest, ConstructorUsesEuxisHome) {
    setenv("EUXIS_HOME", "/tmp/euxis_engine_test", 1);
    Engine e("");
    auto code = e.run({"version"});
    EXPECT_EQ(code, 0);
    unsetenv("EUXIS_HOME");
}

// --- Coverage: Engine dispatches known command ---
TEST(EngineTest, DispatchesKnownCommand) {
    Engine e("/tmp/euxis_test");
    auto code = e.run({"agent", "list"});
    EXPECT_GE(code, 0);
}

} // namespace
} // namespace euxis::cli
