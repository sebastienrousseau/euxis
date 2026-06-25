/// @file
/// @brief Testable entry point for the `euxis-gateway` binary.
///
/// `apps/gateway/src/main.cpp`'s body lives in `gateway_main` so unit
/// tests can drive argument and environment handling — argv config-
/// path override, EUXIS_HOME / HOME fallback, missing-config detection —
/// without booting the actual HTTP server (which would block).

#pragma once

namespace euxis::gateway {

/// Resolve the config path, validate it exists, and return the exit code
/// that `main` would return. When `start_server` is true (default),
/// boots the gateway after resolution; tests pass `false` to exit before
/// `server.start()` blocks.
[[nodiscard]] int gateway_main(int argc, char* argv[], bool start_server = true);

} // namespace euxis::gateway
