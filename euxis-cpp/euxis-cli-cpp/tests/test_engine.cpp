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

} // namespace
} // namespace euxis::cli
