#include <gtest/gtest.h>
#include "euxis/cli/box.hpp"

namespace euxis::cli::terminal {
namespace {

TEST(BoxTest, PrintBoxEmpty) {
    // Should not crash with empty inputs
    print_box("", "");
}

TEST(BoxTest, PrintBoxWithTitle) {
    // Should not crash
    print_box("Status", "All systems operational");
}

TEST(BoxTest, PrintBoxMultilineBody) {
    // Should not crash
    print_box("Report", "Line 1\nLine 2\nLine 3");
}

TEST(BoxTest, PrintBoxedTableEmpty) {
    // Should not crash with empty headers
    print_boxed_table({}, {});
}

TEST(BoxTest, PrintBoxedTableBasic) {
    std::vector<std::string> headers = {"Name", "Status", "Version"};
    std::vector<TableRow> rows = {
        {{"agent-1", "running", "v1.2.0"}},
        {{"agent-2", "stopped", "v1.1.0"}},
    };
    // Should not crash
    print_boxed_table(headers, rows);
}

TEST(BoxTest, PrintBoxedTableSingleColumn) {
    std::vector<std::string> headers = {"Name"};
    std::vector<TableRow> rows = {{{"alpha"}}, {{"beta"}}};
    // Should not crash
    print_boxed_table(headers, rows);
}

TEST(BoxTest, PrintBoxedTableMissingCells) {
    std::vector<std::string> headers = {"A", "B", "C"};
    std::vector<TableRow> rows = {
        {{"only-one"}},  // Missing B and C cells
    };
    // Should not crash — handles short rows gracefully
    print_boxed_table(headers, rows);
}

// ---------------------------------------------------------------------------
// ASCII fallback path: when caps().unicode is false, box_chars() returns ASCII.
// We cannot change the static caps() at runtime, but we can at least exercise
// the print functions and confirm they do not crash in the current environment.
// On a non-UTF-8 locale, this will exercise the ASCII path (line 25).
// ---------------------------------------------------------------------------
TEST(BoxTest, PrintBoxAsciiFallbackDoesNotCrash) {
    // Exercises print_box; on ASCII terminals this hits the fallback BoxChars.
    print_box("ASCII Test", "Should work on any terminal");
}

TEST(BoxTest, PrintBoxedTableAsciiFallbackDoesNotCrash) {
    std::vector<std::string> headers = {"Col1", "Col2"};
    std::vector<TableRow> rows = {{{"val1", "val2"}}};
    print_boxed_table(headers, rows);
}

// --- Coverage: line 23 (box_chars ASCII fallback when unicode is false) ---
// Note: box_chars() is an internal function called by print_box/print_boxed_table.
// Since caps().unicode is a static const determined at process start, we exercise
// both paths by calling print_box in environments with/without UTF-8.
// The tests below ensure the functions work correctly in the current environment.
TEST(BoxTest, PrintBoxLongTitle) {
    // Exercise with a title longer than terminal width
    std::string long_title(200, 'X');
    print_box(long_title, "Short body");
}

TEST(BoxTest, PrintBoxedTableManyColumns) {
    std::vector<std::string> headers = {"A", "B", "C", "D", "E"};
    std::vector<TableRow> rows = {
        {{"1", "2", "3", "4", "5"}},
        {{"a", "b", "c", "d", "e"}}
    };
    print_boxed_table(headers, rows);
}

TEST(BoxTest, PrintBoxedTableEmptyBody) {
    std::vector<std::string> headers = {"Col"};
    std::vector<TableRow> rows;
    print_boxed_table(headers, rows);
}

TEST(BoxTest, PrintBoxNoTitle) {
    print_box("", "Just body content\nwith multiple lines");
}

} // namespace
} // namespace euxis::cli::terminal
