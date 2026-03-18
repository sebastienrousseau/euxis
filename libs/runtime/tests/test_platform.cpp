#include "euxis/runtime/platform.hpp"

#include <gtest/gtest.h>

namespace euxis::runtime {
namespace {

TEST(RuntimePlatformTest, CurrentReturnsValidInfo) {
    const auto& plat = Platform::current();
    const auto& info = plat.info();

    // OS should be detected (not Unknown on any supported CI/dev machine)
    EXPECT_NE(info.os, OSType::Unknown);
    EXPECT_FALSE(info.os_name.empty());
}

TEST(RuntimePlatformTest, ArchDetected) {
    const auto& info = Platform::current().info();
    EXPECT_NE(info.arch, ArchType::Unknown);
    EXPECT_FALSE(info.arch_name.empty());
}

TEST(RuntimePlatformTest, SingletonReturnsConsistentInstance) {
    const auto& a = Platform::current();
    const auto& b = Platform::current();
    EXPECT_EQ(&a, &b);
}

TEST(RuntimePlatformTest, OpenUrlRejectsShellMetachars) {
    const auto& plat = Platform::current();
    // Injection attempts should be rejected
    EXPECT_FALSE(plat.open_url("https://evil.com'; rm -rf /"));
    EXPECT_FALSE(plat.open_url("https://evil.com`whoami`"));
    EXPECT_FALSE(plat.open_url("https://evil.com$(id)"));
    EXPECT_FALSE(plat.open_url("https://evil.com|cat /etc/passwd"));
    EXPECT_FALSE(plat.open_url("https://evil.com;ls"));
}

TEST(RuntimePlatformTest, PlatformInfoDefaults) {
    PlatformInfo info;
    EXPECT_EQ(info.os, OSType::Unknown);
    EXPECT_EQ(info.arch, ArchType::Unknown);
    EXPECT_FALSE(info.has_sandbox);
    EXPECT_FALSE(info.has_nsjail);
    EXPECT_FALSE(info.is_container);
}

} // namespace
} // namespace euxis::runtime
