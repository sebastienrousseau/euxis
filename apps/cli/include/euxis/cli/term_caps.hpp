#pragma once

#include <string>

namespace euxis::cli::terminal {

struct TermCaps {
    int cols{80};
    int rows{24};
    bool truecolor{false};
    bool sixel{false};
    bool kitty{false};
    bool unicode{true};
    bool no_color{false};
    bool iterm2{false};
    bool wsl{false};
};

/// Detect terminal capabilities from ioctl + environment variables.
[[nodiscard]] auto detect() -> TermCaps;

/// Return cached capabilities (detected once at first call).
[[nodiscard]] auto caps() -> const TermCaps&;

} // namespace euxis::cli::terminal
