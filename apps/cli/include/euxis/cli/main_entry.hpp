/// @file
/// @brief Testable entry point for the `euxis` CLI binary.
///
/// `apps/cli/src/main.cpp`'s body lives in `cli_main` so unit tests
/// can drive the full bootstrap path — libsodium init, EUXIS_HOME
/// resolution from environment, i18n catalog load, and the engine
/// dispatch — without invoking the binary as a subprocess.
///
/// `main()` is a thin shim that forwards to this function.

#pragma once

namespace euxis::cli {

/// Bootstrap and dispatch the CLI. Returns the process exit code.
[[nodiscard]] int cli_main(int argc, char* argv[]);

} // namespace euxis::cli
