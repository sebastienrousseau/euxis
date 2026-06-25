/// @file
/// @brief Testable entry point for the `euxis-publisher` binary.
///
/// `apps/publisher/src/main.cpp`'s body lives in `publisher_main` so
/// unit tests can drive subcommand dispatch (render / build), arg
/// parsing (`--doc`, `--format`, `--mode`), and error paths
/// (missing `--doc`, unknown command, missing content root) without
/// invoking the binary as a subprocess.

#pragma once

#include <ostream>

namespace euxis::publisher {

/// Dispatch the publisher subcommand. Returns the exit code.
/// `out` / `err` default to `std::cout` / `std::cerr` but tests pass
/// `std::ostringstream` to capture rendered output and error lines.
[[nodiscard]] int publisher_main(int argc, char* argv[], std::ostream& out, std::ostream& err);

[[nodiscard]] int publisher_main(int argc, char* argv[]);

} // namespace euxis::publisher
