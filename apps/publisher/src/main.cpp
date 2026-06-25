#include "euxis/publisher/main_entry.hpp"

// Top-level fail-fast: see apps/cli/src/main.cpp for the rationale.
// NOLINTBEGIN(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    return euxis::publisher::publisher_main(argc, argv);
}
// NOLINTEND(bugprone-exception-escape)
