#include <euxis/etx/theme.hpp>

#include <QSignalSpy>
#include <gtest/gtest.h>

namespace euxis::etx {
namespace {

class ThemeEngineTest : public ::testing::Test {
protected:
    ThemeEngine engine_;
};

TEST_F(ThemeEngineTest, DefaultThemeIsLiquidGlass) {
    EXPECT_EQ(engine_.current_theme(), "liquid-glass");
}

TEST_F(ThemeEngineTest, AvailableThemesCount) {
    EXPECT_EQ(engine_.available_themes().size(), 3);
}

TEST_F(ThemeEngineTest, AvailableThemesContents) {
    auto themes = engine_.available_themes();
    EXPECT_TRUE(themes.contains("liquid-glass"));
    EXPECT_TRUE(themes.contains("calm"));
    EXPECT_TRUE(themes.contains("focused"));
}

TEST_F(ThemeEngineTest, ApplyInvalidThemeDoesNotChange) {
    engine_.apply_theme("nonexistent");
    EXPECT_EQ(engine_.current_theme(), "liquid-glass");
}

TEST_F(ThemeEngineTest, ApplyValidThemeChanges) {
    engine_.apply_theme("calm");
    EXPECT_EQ(engine_.current_theme(), "calm");
}

TEST_F(ThemeEngineTest, ApplyThemeEmitsSignal) {
    QSignalSpy spy(&engine_, &ThemeEngine::theme_applied);
    engine_.apply_theme("focused");
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "focused");
}

TEST_F(ThemeEngineTest, CycleThemeAdvances) {
    // Default is liquid-glass (index 0), cycle should go to calm (index 1)
    engine_.cycle_theme();
    EXPECT_EQ(engine_.current_theme(), "calm");
}

TEST_F(ThemeEngineTest, CycleThemeWrapsAround) {
    engine_.apply_theme("focused"); // index 2
    engine_.cycle_theme();          // wraps to index 0
    EXPECT_EQ(engine_.current_theme(), "liquid-glass");
}

TEST_F(ThemeEngineTest, LoadStylesheetValidTheme) {
    auto stylesheet = engine_.load_stylesheet("liquid-glass");
    // Resources are compiled into test binary — stylesheet should be non-empty
    EXPECT_FALSE(stylesheet.isEmpty());
}

TEST_F(ThemeEngineTest, LoadStylesheetInvalidReturnsEmpty) {
    auto stylesheet = engine_.load_stylesheet("nonexistent");
    EXPECT_TRUE(stylesheet.isEmpty());
}

TEST_F(ThemeEngineTest, InvalidApplyDoesNotEmitSignal) {
    QSignalSpy spy(&engine_, &ThemeEngine::theme_applied);
    engine_.apply_theme("bogus-theme");
    EXPECT_EQ(spy.count(), 0);
}

} // namespace
} // namespace euxis::etx
