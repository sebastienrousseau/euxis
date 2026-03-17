#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <vector>

namespace euxis::runtime {

enum class OSType {
    Unknown,
    Linux,
    MacOS,
    Windows,
    WSL1,
    WSL2
};

enum class ArchType {
    Unknown,
    X86_64,
    AArch64,
    Arm
};

struct PlatformInfo {
    OSType os{OSType::Unknown};
    ArchType arch{ArchType::Unknown};
    std::string os_name;
    std::string arch_name;
    bool has_sandbox{false};
    bool has_nsjail{false};
    bool is_container{false};
};

class Platform {
public:
    virtual ~Platform() = default;

    [[nodiscard]] virtual const PlatformInfo& info() const = 0;
    
    // Common OS operations
    [[nodiscard]] virtual bool open_url(std::string_view url) const = 0;
    [[nodiscard]] virtual bool clipboard_copy(std::string_view text) const = 0;
    [[nodiscard]] virtual std::string clipboard_paste() const = 0;

    // Singleton-like access to the current platform
    [[nodiscard]] static const Platform& current();

protected:
    Platform() = default;
};

} // namespace euxis::runtime
