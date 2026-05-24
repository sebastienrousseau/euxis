#include "euxis/cli/tui/color_system.hpp"
#include "euxis/cli/terminal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>

namespace euxis::cli::tui {

// ---------------------------------------------------------------------------
// APCA (Advanced Perceptual Contrast Algorithm) implementation
// Based on SAPC-APCA 0.1.9 (W3C WCAG 3.0 draft)
// ---------------------------------------------------------------------------

namespace {

// sRGB -> linear (gamma decode)
double srgb_to_linear(uint8_t c) {
    double s = c / 255.0;
    return (s <= 0.04045) ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}

// APCA coefficients (SAPC-APCA 0.1.9)
constexpr double Ntx = 0.57;   // Normal text exponent
constexpr double Nbg = 0.56;   // Normal bg exponent
constexpr double Rtx = 0.62;   // Reverse text exponent
constexpr double Rbg = 0.65;   // Reverse bg exponent
constexpr double Scale = 1.14;
constexpr double Offset = 0.027;
constexpr double Clamp = 0.1;

// Soft clamp for low luminance
double soft_clamp(double y) {
    return (y < 0.0) ? 0.0 : (y < Clamp ? y + std::pow(Clamp - y, 1.45) : y);
}

// APCA Y (luminance) from sRGB
double apca_y(RGB c) {
    return 0.2126729 * srgb_to_linear(c.r) +
           0.7151522 * srgb_to_linear(c.g) +
           0.0721750 * srgb_to_linear(c.b);
}

} // namespace

double APCA::lightness(RGB c) {
    return apca_y(c) * 100.0;
}

double APCA::contrast(RGB text, RGB bg) {
    double yt = soft_clamp(apca_y(text));
    double yb = soft_clamp(apca_y(bg));

    double lc = 0.0;
    if (yb > yt) {
        // Normal polarity (light text on dark bg)
        lc = (std::pow(yb, Nbg) - std::pow(yt, Ntx)) * Scale;
    } else {
        // Reverse polarity (dark text on light bg)
        lc = (std::pow(yb, Rbg) - std::pow(yt, Rtx)) * Scale;
    }

    if (std::abs(lc) < Offset) return 0.0;
    return (lc > 0) ? (lc - Offset) * 100.0 : (lc + Offset) * 100.0;
}

bool APCA::meets_gold(RGB text, RGB bg, double min_lc) {
    return std::abs(contrast(text, bg)) >= min_lc;
}

RGB APCA::enforce_contrast(RGB fg, RGB bg, double min_lc) {
    if (meets_gold(fg, bg, min_lc)) return fg;

    // Determine direction: if bg is dark, lighten fg; if bg is light, darken fg.
    double bg_y = apca_y(bg);
    bool lighten = (bg_y < 0.5);

    RGB adjusted = fg;
    for (int step = 0; step < 255; ++step) {
        if (lighten) {
            adjusted.r = static_cast<uint8_t>(std::min(255, adjusted.r + 1));
            adjusted.g = static_cast<uint8_t>(std::min(255, adjusted.g + 1));
            adjusted.b = static_cast<uint8_t>(std::min(255, adjusted.b + 1));
        } else {
            adjusted.r = static_cast<uint8_t>(std::max(0, adjusted.r - 1));
            adjusted.g = static_cast<uint8_t>(std::max(0, adjusted.g - 1));
            adjusted.b = static_cast<uint8_t>(std::max(0, adjusted.b - 1));
        }
        if (meets_gold(adjusted, bg, min_lc)) return adjusted;
    }
    return adjusted; // Best effort
}

// ---------------------------------------------------------------------------
// Palette definitions
// ---------------------------------------------------------------------------

namespace {

struct PaletteDef {
    RGB colors[10]; // Indexed by SemanticRole
};

// Tokyo Dark palette (raw, pre-APCA enforcement)
constexpr PaletteDef tokyo_dark = {{
    {169, 177, 214},  // Primary (tokyo_text)
    {113, 153, 238},  // Secondary (tokyo_blue)
    {56, 168, 157},   // Accent (tokyo_cyan)
    {68, 75, 106},    // Muted (tokyo_dim)
    {115, 218, 202},  // Success (tokyo green-ish)
    {224, 175, 104},  // Warning (tokyo yellow-ish)
    {247, 118, 142},  // Error (tokyo_error)
    {125, 174, 163},  // Info
    {26, 27, 38},     // Surface (bg)
    {192, 202, 245},  // OnSurface (bright text)
}};

// Catppuccin Mocha palette
constexpr PaletteDef catppuccin_mocha = {{
    {205, 214, 244},  // Primary (text)
    {137, 180, 250},  // Secondary (blue)
    {148, 226, 213},  // Accent (teal)
    {88, 91, 112},    // Muted (overlay0)
    {166, 227, 161},  // Success (green)
    {249, 226, 175},  // Warning (yellow)
    {243, 139, 168},  // Error (red)
    {116, 199, 236},  // Info (sapphire)
    {30, 30, 46},     // Surface (base)
    {205, 214, 244},  // OnSurface (text)
}};

// High-contrast palette (meets AAA+ at all sizes)
constexpr PaletteDef high_contrast = {{
    {255, 255, 255},  // Primary
    {130, 190, 255},  // Secondary
    {100, 255, 218},  // Accent
    {160, 160, 180},  // Muted
    {100, 255, 100},  // Success
    {255, 230, 100},  // Warning
    {255, 120, 120},  // Error
    {120, 200, 255},  // Info
    {0, 0, 0},        // Surface
    {255, 255, 255},  // OnSurface
}};

} // namespace

// ---------------------------------------------------------------------------
// ColorSystem
// ---------------------------------------------------------------------------

void ColorSystem::load_palette(const std::string& name) {
    palette_name_ = name;
    cache_valid_ = false;

    const PaletteDef* def = &tokyo_dark;
    if (name == "catppuccin-mocha") def = &catppuccin_mocha;
    else if (name == "high-contrast") def = &high_contrast;

    for (int i = 0; i < 10; ++i) {
        raw_palette_[static_cast<size_t>(i)] = def->colors[i];
    }
}

void ColorSystem::set_background(RGB bg) {
    bg_ = bg;
    cache_valid_ = false;
}

RGB ColorSystem::color(SemanticRole role) const {
    if (!cache_valid_) rebuild_cache();
    return cache_[static_cast<size_t>(role)];
}

std::string ColorSystem::paint(SemanticRole role, std::string_view text) const {
    if (!terminal::colors_enabled()) return std::string(text);
    auto c = color(role);
    return std::format("\033[38;2;{};{};{}m{}\033[0m", c.r, c.g, c.b, text);
}

std::string ColorSystem::paint_rgb(RGB fg, std::string_view text) {
    if (!terminal::colors_enabled()) return std::string(text);
    return std::format("\033[38;2;{};{};{}m{}\033[0m", fg.r, fg.g, fg.b, text);
}

void ColorSystem::rebuild_cache() const {
    for (size_t i = 0; i < 10; ++i) {
        auto role = static_cast<SemanticRole>(i);
        RGB raw = raw_palette_[i];

        // Surface colors are not contrast-enforced (they are backgrounds)
        if (role == SemanticRole::Surface) {
            cache_[i] = raw;
            continue;
        }

        // Muted gets a lower threshold (decorative/non-essential)
        double min_lc = (role == SemanticRole::Muted) ? 45.0 : 75.0;
        cache_[i] = APCA::enforce_contrast(raw, bg_, min_lc);
    }
    cache_valid_ = true;
}

} // namespace euxis::cli::tui
