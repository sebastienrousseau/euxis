#include <gtest/gtest.h>
#include "euxis/cli/terminal.hpp"

namespace euxis::cli::terminal {
namespace {

TEST(TerminalTest, ColorFunctions) {
    // These should not crash, regardless of terminal state
    auto b = bold("test");
    auto r = red("error");
    auto g = green("ok");
    auto y = yellow("warn");
    auto bl = blue("info");
    auto c = cyan("cmd");
    auto m = magenta("special");
    auto d = dim("faint");

    EXPECT_FALSE(b.empty());
    EXPECT_FALSE(r.empty());
    EXPECT_FALSE(g.empty());
    EXPECT_FALSE(y.empty());
    EXPECT_FALSE(bl.empty());
    EXPECT_FALSE(c.empty());
    EXPECT_FALSE(m.empty());
    EXPECT_FALSE(d.empty());
}

TEST(TerminalTest, StatusIcons) {
    EXPECT_FALSE(icon_ok().empty());
    EXPECT_FALSE(icon_fail().empty());
    EXPECT_FALSE(icon_warn().empty());
    EXPECT_FALSE(icon_skip().empty());
    EXPECT_FALSE(icon_info().empty());
}

TEST(TerminalTest, PrintTableEmpty) {
    // Should not crash with empty data
    print_table({}, {});
}

TEST(TerminalTest, PrintTableBasic) {
    std::vector<std::string> headers = {"Name", "Value"};
    std::vector<TableRow> rows = {{{"foo", "bar"}}, {{"baz", "qux"}}};
    // Should not crash
    print_table(headers, rows);
}

TEST(TerminalTest, SpinnerFrame) {
    // Should not crash
    spinner_frame(0, "loading");
    spinner_frame(5, "working");
    spinner_frame(100, "wrapping"); // wraps around
    spinner_clear();
}

TEST(TerminalTest, IsCI) {
    // Just verify it returns a bool
    [[maybe_unused]] bool ci = is_ci();
}

} // namespace
} // namespace euxis::cli::terminal
