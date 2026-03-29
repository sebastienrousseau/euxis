/// @file
/// @brief Seccomp-BPF sandboxing for child processes.
#pragma once

#include <string>

namespace euxis::platform {

enum class SandboxProfile { Default, Permissive, Strict };

struct SandboxResult {
    bool applied{false};
    std::string error;
};

/// Check if seccomp is available on the current platform.
[[nodiscard]] auto seccomp_available() -> bool;

/// Apply a seccomp-BPF filter to the current process.
/// Must be called after fork(), before exec().
[[nodiscard]] auto apply_seccomp(SandboxProfile profile = SandboxProfile::Default) -> SandboxResult;

} // namespace euxis::platform
