#include "euxis/cli/tui/platform.hpp"
#include <euxis/runtime/platform.hpp>
#include <cstdlib>

namespace euxis::cli::tui {

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
