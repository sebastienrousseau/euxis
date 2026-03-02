#include "euxis/cli/term_caps.hpp"

#include <cstdlib>
#include <cstring>
#include <string_view>

#include <langinfo.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace euxis::cli::terminal {

auto detect() -> TermCaps {
    TermCaps tc;

    // Terminal size via ioctl
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_col > 0) tc.cols = ws.ws_col;
        if (ws.ws_row > 0) tc.rows = ws.ws_row;
    }

    // 24-bit truecolor detection
    const char* colorterm = std::getenv("COLORTERM");
    if (colorterm) {
        std::string_view ct(colorterm);
        tc.truecolor = (ct == "truecolor" || ct == "24bit");
    }

    // Kitty graphics protocol
    const char* term = std::getenv("TERM");
    const char* kitty_id = std::getenv("KITTY_WINDOW_ID");
    if (kitty_id) {
        tc.kitty = true;
    } else if (term && std::string_view(term).find("kitty") != std::string_view::npos) {
        tc.kitty = true;
    }

    // Sixel support
    if (term) {
        std::string_view tv(term);
        tc.sixel = (tv.find("sixel") != std::string_view::npos);
    }
    const char* sixel_env = std::getenv("SIXEL_SUPPORT");
    if (sixel_env && std::string_view(sixel_env) == "1") {
        tc.sixel = true;
    }

    // Unicode detection
    const char* codeset = nl_langinfo(CODESET);
    if (codeset) {
        std::string_view cs(codeset);
        tc.unicode = (cs == "UTF-8" || cs == "UTF8" || cs == "utf-8" || cs == "utf8");
    } else {
        // Fall back to LANG
        const char* lang = std::getenv("LANG");
        if (lang) {
            tc.unicode = (std::string_view(lang).find("UTF") != std::string_view::npos);
        } else {
            tc.unicode = false;
        }
    }

    return tc;
}

auto caps() -> const TermCaps& {
    static const TermCaps instance = detect();
    return instance;
}

} // namespace euxis::cli::terminal
