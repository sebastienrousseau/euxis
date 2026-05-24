#pragma once

#include <string>

namespace euxis::bridge {

struct PlatformInfo {
    std::string os_name;     // "linux", "macos", "windows"
    std::string arch;        // "x86_64", "aarch64"
    bool has_nsjail = false;
    bool has_sandbox_exec = false;
};

/// Detect platform capabilities
[[nodiscard]] auto detect_platform() -> PlatformInfo;

/// Check if nsjail is available on PATH
[[nodiscard]] auto has_nsjail() -> bool;

}  // namespace euxis::bridge
