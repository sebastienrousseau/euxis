#include "euxis/cli/tui/platform.hpp"
#include <euxis/runtime/platform.hpp>

#include <cstdlib>
#include <memory>

namespace euxis::cli::tui {

PlatformType Platform::detect_type() {
    using CoreOS = euxis::runtime::OSType;
    const auto& info = euxis::runtime::Platform::current().info();
    
    if (info.os == CoreOS::MacOS) return PlatformType::MacOS;
    if (info.os == CoreOS::WSL1 || info.os == CoreOS::WSL2) return PlatformType::WSL;
    return PlatformType::Linux;
}

std::unique_ptr<Platform> Platform::detect() {
    switch (detect_type()) {
        case PlatformType::MacOS: return std::make_unique<MacOSPlatform>();
        case PlatformType::WSL:   return std::make_unique<WSLPlatform>();
        default:                  return std::make_unique<LinuxPlatform>();
    }
}

// ---------------------------------------------------------------------------
// LinuxPlatform
// ---------------------------------------------------------------------------

bool LinuxPlatform::has_wayland() const {
    return std::getenv("WAYLAND_DISPLAY") != nullptr;
}

bool LinuxPlatform::clipboard_copy(std::string_view text) const {
    return euxis::runtime::Platform::current().clipboard_copy(text);
}

std::string LinuxPlatform::clipboard_paste() const {
    return euxis::runtime::Platform::current().clipboard_paste();
}

bool LinuxPlatform::open_url(std::string_view url) const {
    return euxis::runtime::Platform::current().open_url(url);
}

bool LinuxPlatform::supports_truecolor() const {
    const char* ct = std::getenv("COLORTERM");
    if (ct) {
        std::string_view sv(ct);
        return sv == "truecolor" || sv == "24bit";
    }
    return false;
}

} // namespace euxis::cli::tui
