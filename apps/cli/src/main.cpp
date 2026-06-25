#include "euxis/cli/main_entry.hpp"

// Top-level fail-fast: if the CLI bootstrap throws (allocator failure, FS
// unwritable), terminate is the documented exit path. The C++ runtime emits
// the exception details via the OS abort handler.
// NOLINTBEGIN(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    return euxis::cli::cli_main(argc, argv);
}
// NOLINTEND(bugprone-exception-escape)
