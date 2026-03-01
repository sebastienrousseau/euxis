#include <euxis/bridge/platform.hpp>

#include <cstdlib>

#ifdef __linux__
#include <sys/utsname.h>
#endif

namespace euxis::bridge {

auto detect_platform() -> PlatformInfo {
    PlatformInfo info;

#if defined(__linux__)
    info.os_name = "linux";
    info.has_sandbox_exec = false;
#elif defined(__APPLE__)
    info.os_name = "macos";
    info.has_sandbox_exec = true;
#elif defined(_WIN32)
    info.os_name = "windows";
    info.has_sandbox_exec = false;
#else
    info.os_name = "unknown";
    info.has_sandbox_exec = false;
#endif

#if defined(__x86_64__) || defined(_M_X64)
    info.arch = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    info.arch = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
    info.arch = "arm";
#else
    info.arch = "unknown";
#endif

    info.has_nsjail = has_nsjail();
    return info;
}

auto has_nsjail() -> bool {
    int result = std::system("which nsjail > /dev/null 2>&1");
    return result == 0;
}

}  // namespace euxis::bridge
