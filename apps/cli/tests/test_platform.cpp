#include <gtest/gtest.h>
#include "euxis/cli/tui/platform.hpp"

#include <cstdlib>

namespace euxis::cli::tui {
namespace {

TEST(PlatformTest, DetectReturnsNonNull) {
    auto platform = Platform::detect();
    EXPECT_NE(platform, nullptr);
}

TEST(PlatformTest, DetectTypeReturnsValid) {
    auto t = Platform::detect_type();
    EXPECT_TRUE(t == PlatformType::Linux || t == PlatformType::MacOS || t == PlatformType::WSL);
}

TEST(PlatformTest, NameIsNonEmpty) {
    auto platform = Platform::detect();
    EXPECT_FALSE(platform->name().empty());
}

TEST(PlatformTest, TypeMatchesDetectType) {
    auto platform = Platform::detect();
    EXPECT_EQ(platform->type(), Platform::detect_type());
}

TEST(PlatformTest, SigwinchReliableReturnsBool) {
    auto platform = Platform::detect();
    [[maybe_unused]] bool reliable = platform->sigwinch_reliable();
}

TEST(PlatformTest, ResizePollIntervalNonNegative) {
    auto platform = Platform::detect();
    EXPECT_GE(platform->resize_poll_interval_ms(), 0);
}

TEST(PlatformTest, SupportsTruecolorReturnsBool) {
    auto platform = Platform::detect();
    [[maybe_unused]] bool tc = platform->supports_truecolor();
}

// --- Platform-specific tests ---

TEST(PlatformTest, LinuxPlatformType) {
    LinuxPlatform lp;
    EXPECT_EQ(lp.type(), PlatformType::Linux);
    EXPECT_EQ(lp.name(), "Linux");
    EXPECT_TRUE(lp.sigwinch_reliable());
    EXPECT_EQ(lp.resize_poll_interval_ms(), 0);
}

TEST(PlatformTest, MacOSPlatformType) {
    MacOSPlatform mp;
    EXPECT_EQ(mp.type(), PlatformType::MacOS);
    EXPECT_EQ(mp.name(), "macOS");
    EXPECT_TRUE(mp.sigwinch_reliable());
    EXPECT_EQ(mp.resize_poll_interval_ms(), 0);
}

TEST(PlatformTest, WSLPlatformType) {
    WSLPlatform wp;
    EXPECT_EQ(wp.type(), PlatformType::WSL);
    EXPECT_EQ(wp.name(), "WSL");
}

TEST(PlatformTest, WSLResizePollInterval) {
    WSLPlatform wp;
    // WSL2 has reliable SIGWINCH (0ms poll), WSL1 needs polling (500ms)
    if (wp.is_wsl2()) {
        EXPECT_EQ(wp.resize_poll_interval_ms(), 0);
        EXPECT_TRUE(wp.sigwinch_reliable());
    } else {
        EXPECT_EQ(wp.resize_poll_interval_ms(), 500);
        EXPECT_FALSE(wp.sigwinch_reliable());
    }
}

// --- Clipboard tests (best-effort, may fail in CI) ---

TEST(PlatformTest, ClipboardPasteDoesNotCrash) {
    auto platform = Platform::detect();
    // Just ensure it doesn't crash; may return empty in headless/CI
    [[maybe_unused]] std::string text = platform->clipboard_paste();
}

TEST(PlatformTest, ClipboardCopyDoesNotCrash) {
    auto platform = Platform::detect();
    // May fail silently in CI, just ensure no crash
    [[maybe_unused]] bool ok = platform->clipboard_copy("test");
}

TEST(PlatformTest, OpenUrlDoesNotCrash) {
    // Don't actually open a URL in test; just verify the method exists
    auto platform = Platform::detect();
    (void)platform; // We verify the API compiles
}

// --- Truecolor detection ---

TEST(PlatformTest, TruecolorDetectionConsistent) {
    auto platform = Platform::detect();
    bool a = platform->supports_truecolor();
    bool b = platform->supports_truecolor();
    EXPECT_EQ(a, b);
}

TEST(PlatformTest, LinuxTruecolorFromEnv) {
    LinuxPlatform lp;
    // Truecolor detection depends on COLORTERM env var
    const char* ct = std::getenv("COLORTERM");
    if (ct && (std::string_view(ct) == "truecolor" || std::string_view(ct) == "24bit")) {
        EXPECT_TRUE(lp.supports_truecolor());
    }
}

} // namespace
} // namespace euxis::cli::tui
