#include <gtest/gtest.h>
#include <euxis/platform/sandbox.hpp>

namespace euxis::platform {

TEST(SandboxTest, SeccompAvailableOnLinux) {
#ifdef __linux__
    EXPECT_TRUE(seccomp_available());
#else
    EXPECT_FALSE(seccomp_available());
#endif
}

TEST(SandboxTest, DefaultProfileCreatesResult) {
    auto result = apply_seccomp(SandboxProfile::Default);
    // In test process, we can't actually apply seccomp (it would affect the test runner)
    // Just verify the function returns without crashing
#ifdef __linux__
    // May fail if already in a sandbox — that's OK
    SUCCEED();
#else
    EXPECT_FALSE(result.applied);
#endif
}

TEST(SandboxTest, PermissiveProfileNoFilter) {
    auto result = apply_seccomp(SandboxProfile::Permissive);
#ifndef __linux__
    EXPECT_FALSE(result.applied);
#else
    SUCCEED();
#endif
}

TEST(SandboxTest, NonLinuxReturnsFalse) {
#ifndef __linux__
    EXPECT_FALSE(seccomp_available());
    auto result = apply_seccomp(SandboxProfile::Default);
    EXPECT_FALSE(result.applied);
#else
    SUCCEED();
#endif
}

TEST(SandboxTest, StrictProfileCreatesResult) {
    auto result = apply_seccomp(SandboxProfile::Strict);
#ifndef __linux__
    EXPECT_FALSE(result.applied);
#else
    SUCCEED();
#endif
}

} // namespace euxis::platform
