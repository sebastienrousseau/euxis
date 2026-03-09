#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace euxis::cli::tui {

/// 24-bit RGB color.
struct RGB {
    uint8_t r{0}, g{0}, b{0};
    bool operator==(const RGB& o) const = default;
};

/// Semantic color roles for theming.
enum class SemanticRole {
    Primary,     // Main foreground text
    Secondary,   // Supporting text
    Accent,      // Highlighted elements
    Muted,       // De-emphasized / dim text
    Success,     // Positive indicators
    Warning,     // Caution indicators
    Error,       // Error indicators
    Info,        // Informational indicators
    Surface,     // Panel / widget background
    OnSurface,   // Text on Surface background
};

/// APCA (Advanced Perceptual Contrast Algorithm) — WCAG 3.0 standard.
/// Replaces WCAG 2.x luminance ratio with perceptual Lc (lightness contrast).
struct APCA {
    /// Compute APCA perceptual lightness from sRGB.
    static double lightness(RGB c);

    /// Compute Lc contrast between text color and background color.
    /// Returns absolute value in range [0, ~108].
    static double contrast(RGB text, RGB bg);

    /// Returns true if the pair meets "Gold" threshold (Lc >= 75 body, >= 60 large).
    static bool meets_gold(RGB text, RGB bg, double min_lc = 75.0);

    /// Lighten (or darken) `fg` until it meets `min_lc` contrast against `bg`.
    /// Returns the adjusted color.
    static RGB enforce_contrast(RGB fg, RGB bg, double min_lc = 75.0);
};

/// Named palette with APCA-enforced semantic colors.
class ColorSystem {
public:
    /// Load a named palette. Supported: "tokyo-dark", "catppuccin-mocha", "high-contrast".
    void load_palette(const std::string& name);

    /// Set the detected terminal background (for APCA enforcement).
    void set_background(RGB bg);

    /// Get the resolved color for a semantic role (APCA-enforced against bg).
    [[nodiscard]] RGB color(SemanticRole role) const;

    /// Paint text with the given semantic role's foreground color (ANSI escape).
    /// Returns plain text when colors are disabled (NO_COLOR / non-TTY).
    [[nodiscard]] std::string paint(SemanticRole role, std::string_view text) const;

    /// Paint text with arbitrary RGB foreground.
    [[nodiscard]] static std::string paint_rgb(RGB fg, std::string_view text);

    /// Returns the current palette name.
    [[nodiscard]] const std::string& palette_name() const { return palette_name_; }

    /// Returns the detected background.
    [[nodiscard]] RGB background() const { return bg_; }

private:
    std::string palette_name_{"tokyo-dark"};
    RGB bg_{0, 0, 0};
    std::array<RGB, 10> raw_palette_{};   // Indexed by SemanticRole
    mutable std::array<RGB, 10> cache_{};
    mutable bool cache_valid_{false};

    void rebuild_cache() const;
};

} // namespace euxis::cli::tui
