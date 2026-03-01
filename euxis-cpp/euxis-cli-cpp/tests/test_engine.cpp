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

} // namespace
} // namespace euxis::cli
