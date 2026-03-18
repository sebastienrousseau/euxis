#include "euxis/platform/platform.hpp"
#include <euxis/runtime/platform.hpp>

#include <cstdlib>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// WSLPlatform
// ---------------------------------------------------------------------------

WSLPlatform::WSLPlatform() {
    using CoreOS = euxis::runtime::OSType;
    const auto& info = euxis::runtime::Platform::current().info();
    is_wsl2_ = (info.os == CoreOS::WSL2);
}

bool WSLPlatform::clipboard_copy(std::string_view text) const {
    return euxis::runtime::Platform::current().clipboard_copy(text);
}

std::string WSLPlatform::clipboard_paste() const {
    return euxis::runtime::Platform::current().clipboard_paste();
}

bool WSLPlatform::open_url(std::string_view url) const {
    return euxis::runtime::Platform::current().open_url(url);
}

bool WSLPlatform::supports_truecolor() const {
    const char* wt = std::getenv("WT_SESSION");
    if (wt) return true;

    const char* ct = std::getenv("COLORTERM");
    if (ct) {
        std::string_view sv(ct);
        return sv == "truecolor" || sv == "24bit";
    }
    return false;
}

} // namespace euxis::cli::tui
