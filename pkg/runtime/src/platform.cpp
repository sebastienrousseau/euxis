#include <euxis/runtime/platform.hpp>
#include <euxis/runtime/detail/process_utils.hpp>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <cctype>

#ifdef __linux__
#include <unistd.h>
#endif

namespace euxis::runtime {

namespace {

class GenericPlatform : public Platform {
public:
    GenericPlatform() {
        detect();
    }

    const PlatformInfo& info() const override { return info_; }

    bool open_url(std::string_view url) const override {
        std::string cmd;
        if (info_.os == OSType::MacOS) {
            cmd = "open '" + std::string(url) + "' 2>/dev/null &";
        } else if (info_.os == OSType::WSL1 || info_.os == OSType::WSL2) {
            if (std::system(("wslview '" + std::string(url) + "' 2>/dev/null").c_str()) == 0) return true;
            cmd = "cmd.exe /c start '" + std::string(url) + "' 2>/dev/null";
        } else {
            cmd = "xdg-open '" + std::string(url) + "' >/dev/null 2>&1 &";
        }
        return std::system(cmd.c_str()) == 0;
    }

    bool clipboard_copy(std::string_view text) const override {
        if (info_.os == OSType::MacOS) return detail::run_pipe_command("pbcopy", text);
        if (info_.os == OSType::WSL1 || info_.os == OSType::WSL2) return detail::run_pipe_command("clip.exe 2>/dev/null", text);
        if (std::getenv("WAYLAND_DISPLAY")) return detail::run_pipe_command("wl-copy 2>/dev/null", text);
        return detail::run_pipe_command("xclip -selection clipboard 2>/dev/null", text);
    }

    std::string clipboard_paste() const override {
        if (info_.os == OSType::MacOS) return detail::run_command("pbpaste");
        if (info_.os == OSType::WSL1 || info_.os == OSType::WSL2) {
            std::string res = detail::run_command("powershell.exe -NoProfile -Command Get-Clipboard 2>/dev/null");
            std::string clean;
            for (char c : res) if (c != '\r') clean += c;
            return clean;
        }
        if (std::getenv("WAYLAND_DISPLAY")) return detail::run_command("wl-paste 2>/dev/null");
        return detail::run_command("xclip -selection clipboard -o 2>/dev/null");
    }

private:
    void detect() {
#ifdef __APPLE__
        info_.os = OSType::MacOS;
        info_.os_name = "macos";
        info_.has_sandbox = true;
#elif defined(_WIN32)
        info_.os = OSType::Windows;
        info_.os_name = "windows";
#else
        if (std::getenv("WSL_DISTRO_NAME")) {
            info_.os = OSType::WSL2;
            info_.os_name = "wsl";
        } else {
            info_.os = OSType::Linux;
            info_.os_name = "linux";
        }

        std::ifstream proc_version("/proc/version");
        if (proc_version.is_open()) {
            std::string line;
            std::getline(proc_version, line);
            std::string lower = line;
            for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
            
            if (lower.find("microsoft") != std::string::npos) {
                info_.os = (lower.find("wsl2") != std::string::npos) ? OSType::WSL2 : OSType::WSL1;
                info_.os_name = "wsl";
            }
        }
#endif

#if defined(__x86_64__) || defined(_M_X64)
        info_.arch = ArchType::X86_64;
        info_.arch_name = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
        info_.arch = ArchType::AArch64;
        info_.arch_name = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
        info_.arch = ArchType::Arm;
        info_.arch_name = "arm";
#endif

        info_.has_nsjail = check_nsjail();
        info_.is_container = (std::ifstream("/.dockerenv").good() || std::ifstream("/run/.containerenv").good());
    }

    bool check_nsjail() const {
        const char* path = std::getenv("PATH");
        if (!path) return false;
        std::string p(path);
        size_t start = 0, end;
        while ((end = p.find(':', start)) != std::string::npos) {
            std::string dir = p.substr(start, end - start);
#ifdef __linux__
            if (access((dir + "/nsjail").c_str(), X_OK) == 0) return true;
#endif
            start = end + 1;
        }
        return false;
    }

    PlatformInfo info_;
};

} // namespace

const Platform& Platform::current() {
    static GenericPlatform instance;
    return instance;
}

} // namespace euxis::runtime
