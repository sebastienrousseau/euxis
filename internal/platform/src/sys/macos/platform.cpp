#include "euxis/cli/tui/platform.hpp"
#include <euxis/runtime/platform.hpp>
#include <cstdlib>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// MacOSPlatform
// ---------------------------------------------------------------------------

bool MacOSPlatform::clipboard_copy(std::string_view text) const {
    return euxis::runtime::Platform::current().clipboard_copy(text);
}

std::string MacOSPlatform::clipboard_paste() const {
    return euxis::runtime::Platform::current().clipboard_paste();
}

bool MacOSPlatform::open_url(std::string_view url) const {
    return euxis::runtime::Platform::current().open_url(url);
}

bool MacOSPlatform::supports_truecolor() const {
    // Terminal.app only does 256 color. iTerm2/Kitty/Alacritty do truecolor.
    const char* term_program = std::getenv("TERM_PROGRAM");
    if (term_program) {
        std::string_view tp(term_program);
        if (tp == "Apple_Terminal") return false;
    }
    const char* ct = std::getenv("COLORTERM");
    if (ct) {
        std::string_view sv(ct);
        return sv == "truecolor" || sv == "24bit";
    }
    return false;
}

} // namespace euxis::cli::tui
