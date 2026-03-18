#include "euxis/platform/platform.hpp"
#include <euxis/runtime/platform.hpp>
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

} // namespace euxis::cli::tui
