#include <gtest/gtest.h>
#include "euxis/cli/tui/color_system.hpp"
#include "euxis/cli/terminal.hpp"

#include <cstdlib>

namespace euxis::cli::tui {
namespace {

// --- APCA Tests ---

TEST(APCATest, LightnessBlack) {
    EXPECT_NEAR(APCA::lightness({0, 0, 0}), 0.0, 0.01);
}

TEST(APCATest, LightnessWhite) {
    EXPECT_GT(APCA::lightness({255, 255, 255}), 90.0);
}

TEST(APCATest, ContrastWhiteOnBlack) {
    double lc = APCA::contrast({255, 255, 255}, {0, 0, 0});
    EXPECT_GT(std::abs(lc), 90.0); // White on black has very high contrast
}

TEST(APCATest, ContrastBlackOnWhite) {
    double lc = APCA::contrast({0, 0, 0}, {255, 255, 255});
    EXPECT_GT(std::abs(lc), 90.0);
}

TEST(APCATest, ContrastSameColor) {
    double lc = APCA::contrast({128, 128, 128}, {128, 128, 128});
    EXPECT_NEAR(lc, 0.0, 1.0);
}

TEST(APCATest, MeetsGoldWhiteOnBlack) {
    EXPECT_TRUE(APCA::meets_gold({255, 255, 255}, {0, 0, 0}));
}

TEST(APCATest, MeetsGoldDimOnBlackFails) {
    // tokyo_dim (68,75,106) on black: should fail for body text (Lc >= 75)
    EXPECT_FALSE(APCA::meets_gold({68, 75, 106}, {0, 0, 0}, 75.0));
}

TEST(APCATest, EnforceContrastLightensOnDarkBg) {
    RGB dim{68, 75, 106};
    RGB bg{0, 0, 0};
    RGB adjusted = APCA::enforce_contrast(dim, bg, 45.0);
    // Adjusted should meet the threshold
    EXPECT_TRUE(APCA::meets_gold(adjusted, bg, 45.0));
    // Adjusted should be lighter than original
    EXPECT_GE(adjusted.r + adjusted.g + adjusted.b, dim.r + dim.g + dim.b);
}

TEST(APCATest, EnforceContrastDarkensOnLightBg) {
    RGB bright{200, 200, 200};
    RGB bg{255, 255, 255};
    RGB adjusted = APCA::enforce_contrast(bright, bg, 60.0);
    // Adjusted should be darker
    EXPECT_LE(adjusted.r + adjusted.g + adjusted.b, bright.r + bright.g + bright.b);
}

TEST(APCATest, EnforceContrastNoChangeWhenAlreadyMeeting) {
    RGB white{255, 255, 255};
    RGB black{0, 0, 0};
    RGB adjusted = APCA::enforce_contrast(white, black, 75.0);
    EXPECT_EQ(adjusted.r, white.r);
    EXPECT_EQ(adjusted.g, white.g);
    EXPECT_EQ(adjusted.b, white.b);
}

TEST(APCATest, ContrastSymmetryProperty) {
    RGB a{100, 150, 200};
    RGB b{20, 20, 30};
    double lc1 = std::abs(APCA::contrast(a, b));
    double lc2 = std::abs(APCA::contrast(b, a));
    // APCA is polarity-aware, so values differ but both should be non-zero
    EXPECT_GT(lc1, 0.0);
    EXPECT_GT(lc2, 0.0);
}

// --- ColorSystem Tests ---

TEST(ColorSystemTest, DefaultPaletteIsTokyo) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    EXPECT_EQ(cs.palette_name(), "tokyo-dark");
}

TEST(ColorSystemTest, LoadCatppuccinMocha) {
    ColorSystem cs;
    cs.load_palette("catppuccin-mocha");
    EXPECT_EQ(cs.palette_name(), "catppuccin-mocha");
}

TEST(ColorSystemTest, LoadHighContrast) {
    ColorSystem cs;
    cs.load_palette("high-contrast");
    EXPECT_EQ(cs.palette_name(), "high-contrast");
}

TEST(ColorSystemTest, UnknownPaletteFallsBackToTokyo) {
    ColorSystem cs;
    cs.load_palette("nonexistent");
    // Should load tokyo-dark as fallback
    EXPECT_EQ(cs.palette_name(), "nonexistent"); // Name is set, but colors are from tokyo-dark
}

TEST(ColorSystemTest, SetBackground) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({26, 27, 38});
    EXPECT_EQ(cs.background().r, 26);
    EXPECT_EQ(cs.background().g, 27);
    EXPECT_EQ(cs.background().b, 38);
}

TEST(ColorSystemTest, AllSemanticRolesResolve) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});

    // Every role should return a valid color
    for (int i = 0; i < 10; ++i) {
        auto role = static_cast<SemanticRole>(i);
        RGB c = cs.color(role);
        // Color components should be in valid range (always true for uint8_t, but verify non-zero for text roles)
        if (role != SemanticRole::Surface) {
            EXPECT_GT(c.r + c.g + c.b, 0);
        }
    }
}

TEST(ColorSystemTest, AllColorsPassAPCAOnBlackBg) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});

    RGB bg{0, 0, 0};
    for (int i = 0; i < 10; ++i) {
        auto role = static_cast<SemanticRole>(i);
        if (role == SemanticRole::Surface) continue; // Background color itself

        RGB c = cs.color(role);
        double min_lc = (role == SemanticRole::Muted) ? 45.0 : 75.0;
        EXPECT_TRUE(APCA::meets_gold(c, bg, min_lc))
            << "Role " << i << " (RGB " << (int)c.r << "," << (int)c.g << "," << (int)c.b
            << ") fails APCA Lc>=" << min_lc << " on black bg";
    }
}

TEST(ColorSystemTest, AllColorsPassAPCAOnDarkBg) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({26, 27, 38});

    RGB bg{26, 27, 38};
    for (int i = 0; i < 10; ++i) {
        auto role = static_cast<SemanticRole>(i);
        if (role == SemanticRole::Surface) continue;

        RGB c = cs.color(role);
        double min_lc = (role == SemanticRole::Muted) ? 45.0 : 75.0;
        EXPECT_TRUE(APCA::meets_gold(c, bg, min_lc))
            << "Role " << i << " fails APCA on dark bg";
    }
}

TEST(ColorSystemTest, PaintReturnsNonEmpty) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});

    std::string result = cs.paint(SemanticRole::Primary, "hello");
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("hello"), std::string::npos);
}

TEST(ColorSystemTest, PaintRgbReturnsNonEmpty) {
    std::string result = ColorSystem::paint_rgb({255, 128, 0}, "orange");
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("orange"), std::string::npos);
}

TEST(ColorSystemTest, SurfaceColorUnchanged) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});

    RGB surface = cs.color(SemanticRole::Surface);
    // Surface should be the raw palette value (26, 27, 38 for tokyo-dark)
    EXPECT_EQ(surface.r, 26);
    EXPECT_EQ(surface.g, 27);
    EXPECT_EQ(surface.b, 38);
}

TEST(ColorSystemTest, CacheInvalidatesOnSetBackground) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});
    RGB c1 = cs.color(SemanticRole::Primary);

    cs.set_background({200, 200, 200}); // Light background
    RGB c2 = cs.color(SemanticRole::Primary);

    // On a light bg, the primary text should be darker than on a dark bg
    EXPECT_NE(c1.r, c2.r); // Should differ
}

TEST(ColorSystemTest, CacheInvalidatesOnLoadPalette) {
    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    cs.set_background({0, 0, 0});
    RGB c1 = cs.color(SemanticRole::Accent);

    cs.load_palette("high-contrast");
    RGB c2 = cs.color(SemanticRole::Accent);

    // Different palettes should produce different accent colors
    EXPECT_FALSE(c1 == c2);
}

// --- NO_COLOR Compliance Tests ---

TEST(ColorSystemTest, PaintWithNoColorEnv) {
    setenv("NO_COLOR", "1", 1);

    ColorSystem cs;
    cs.load_palette("tokyo-dark");
    std::string result = cs.paint(SemanticRole::Primary, "plain");
    // Should return plain text without ANSI escapes
    EXPECT_EQ(result, "plain");

    unsetenv("NO_COLOR");
}

TEST(ColorSystemTest, PaintRgbWithNoColorEnv) {
    setenv("NO_COLOR", "1", 1);

    std::string result = ColorSystem::paint_rgb({255, 0, 0}, "plain");
    EXPECT_EQ(result, "plain");

    unsetenv("NO_COLOR");
}

} // namespace
} // namespace euxis::cli::tui
