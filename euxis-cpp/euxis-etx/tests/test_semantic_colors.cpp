#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>
#include <euxis/etx/semantic_colors.hpp>

namespace euxis::etx {
namespace {

TEST(SemanticColorsTest, SeverityColorReturnsValid) {
    EXPECT_TRUE(severity_color(Severity::Info).isValid());
    EXPECT_TRUE(severity_color(Severity::Success).isValid());
    EXPECT_TRUE(severity_color(Severity::Warning).isValid());
    EXPECT_TRUE(severity_color(Severity::Error).isValid());
    EXPECT_TRUE(severity_color(Severity::Critical).isValid());
}

TEST(SemanticColorsTest, ColorsAreDistinct) {
    auto info = severity_color(Severity::Info);
    auto success = severity_color(Severity::Success);
    auto error = severity_color(Severity::Error);
    EXPECT_NE(info, success);
    EXPECT_NE(info, error);
    EXPECT_NE(success, error);
}

TEST(SemanticColorsTest, QssReturnsNonEmpty) {
    auto qss = severity_qss(Severity::Info);
    EXPECT_FALSE(qss.isEmpty());
    EXPECT_TRUE(qss.contains("background"));
    EXPECT_TRUE(qss.contains("border"));
}

TEST(SemanticColorsTest, StatusToSeverityMapping) {
    EXPECT_EQ(status_to_severity("running"), Severity::Success);
    EXPECT_EQ(status_to_severity("idle"), Severity::Info);
    EXPECT_EQ(status_to_severity("error"), Severity::Error);
    EXPECT_EQ(status_to_severity("warning"), Severity::Warning);
    EXPECT_EQ(status_to_severity("critical"), Severity::Critical);
    EXPECT_EQ(status_to_severity("unknown"), Severity::Info);  // default
}

TEST(SemanticColorsTest, ApplyBadgeNoCrash) {
    auto* label = new QLabel("test");
    apply_severity_badge(label, Severity::Success);
    EXPECT_FALSE(label->styleSheet().isEmpty());
    delete label;
}

TEST(SemanticColorsTest, ApplyPulseNoCrash) {
    auto* label = new QLabel("pulse");
    apply_pulse_animation(label, severity_color(Severity::Critical));
    EXPECT_NE(label->graphicsEffect(), nullptr);
    delete label;
}

TEST(SemanticColorsTest, SeverityColorFallbackForInvalidEnum) {
    // Cast an out-of-range value to Severity to hit the fallback return
    // after the switch statement (line 16: QColor(0x88,0x88,0x88))
    auto color = severity_color(static_cast<Severity>(999));
    EXPECT_TRUE(color.isValid());
    EXPECT_EQ(color.red(), 0x88);
    EXPECT_EQ(color.green(), 0x88);
    EXPECT_EQ(color.blue(), 0x88);
}

TEST(SemanticColorsTest, SeverityQssAllLevels) {
    // Exercise severity_qss for every severity level (covers line 21)
    for (auto sev : {Severity::Info, Severity::Success, Severity::Warning,
                     Severity::Error, Severity::Critical}) {
        auto qss = severity_qss(sev);
        EXPECT_FALSE(qss.isEmpty());
        EXPECT_TRUE(qss.contains("background"));
        EXPECT_TRUE(qss.contains("border-radius"));
        EXPECT_TRUE(qss.contains("color:"));
    }
}

} // namespace
} // namespace euxis::etx
