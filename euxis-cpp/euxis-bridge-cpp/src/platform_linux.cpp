#include <euxis/bridge/platform.hpp>

#include <cstdlib>
#include <cstring>
#include <string>

#ifdef __linux__
#include <sys/utsname.h>
#include <unistd.h>
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
    // Safe PATH search without invoking a shell
    const char* path_env = std::getenv("PATH");
    if (!path_env) return false;

    std::string path_str(path_env);
    std::string::size_type start = 0;
    while (start < path_str.size()) {
        auto end = path_str.find(':', start);
        std::string dir = path_str.substr(start, end == std::string::npos ? end : end - start);
        std::string candidate = dir + "/nsjail";
#ifdef __linux__
        if (access(candidate.c_str(), X_OK) == 0) return true;
#endif
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

}  // namespace euxis::bridge
