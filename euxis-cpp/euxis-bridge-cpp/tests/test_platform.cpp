#include "euxis/bridge/platform.hpp"

#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

namespace euxis::bridge {
namespace {

TEST(PlatformTest, DetectPlatformReturnsLinux) {
    auto info = detect_platform();
#if defined(__linux__)
    EXPECT_EQ(info.os_name, "linux");
#elif defined(__APPLE__)
    EXPECT_EQ(info.os_name, "macos");
#endif
}

TEST(PlatformTest, ArchIsKnown) {
    auto info = detect_platform();
    EXPECT_TRUE(info.arch == "x86_64" || info.arch == "aarch64" || info.arch == "arm");
}

TEST(PlatformTest, HasNsjailReturnsBool) {
    // Just verify it doesn't crash and returns a boolean
    auto result = has_nsjail();
    (void)result;  // may or may not be true depending on system
    SUCCEED();
}

TEST(PlatformTest, DetectPlatformSandboxExec) {
    auto info = detect_platform();
#if defined(__linux__)
    EXPECT_FALSE(info.has_sandbox_exec);
#elif defined(__APPLE__)
    EXPECT_TRUE(info.has_sandbox_exec);
#endif
}

TEST(PlatformTest, DetectPlatformNsjailConsistent) {
    auto info = detect_platform();
    EXPECT_EQ(info.has_nsjail, has_nsjail());
}

TEST(PlatformTest, HasNsjailReturnsFalseWithEmptyPath) {
    // Save and clear PATH to exercise the error path (line 43: path_env == nullptr)
    const char* saved_path = std::getenv("PATH");
    std::string saved;
    if (saved_path) saved = saved_path;

    // Unset PATH to trigger the early return false
    ::unsetenv("PATH");
    EXPECT_FALSE(has_nsjail());

    // Restore PATH
    if (!saved.empty()) {
        ::setenv("PATH", saved.c_str(), 1);
    }
}

} // namespace
} // namespace euxis::bridge
