#include <gtest/gtest.h>
#include <euxis/platform/sandbox.hpp>

#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace euxis::platform {

#ifdef __linux__
namespace {

// A seccomp filter cannot be removed and is inherited, so applying one in
// the test process breaks every later test that forks or execs (issue #96:
// the Strict profile blocks execve). Each profile is applied in a forked
// child; the child reports what the filter did through its exit code.
template <typename Fn>
[[nodiscard]] auto exit_code_in_child(Fn&& fn) -> int {
    const pid_t pid = ::fork();
    if (pid < 0) return -1;
    if (pid == 0) ::_exit(fn());
    int status = 0;
    if (::waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
}

/// 0 if execve succeeds (the child becomes `sh -c "exit 0"`), else errno.
[[nodiscard]] auto exec_true() -> int {
    ::execl("/bin/sh", "sh", "-c", "exit 0", static_cast<char*>(nullptr));
    return errno;
}

enum Code : int { kOk = 0, kNotApplied = 101, kNotBlocked = 102, kWrongState = 103 };

class SandboxLinuxTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!seccomp_available()) {
            GTEST_SKIP() << "seccomp unavailable here (WSL, or an emulator such as QEMU user mode)";
        }
    }
};

} // namespace

TEST(SandboxTest, SeccompAvailableOnLinux) {
    EXPECT_TRUE(seccomp_available());
}

TEST_F(SandboxLinuxTest, DefaultProfileBlocksEscapeSyscallsButAllowsExec) {
    const int code = exit_code_in_child([] {
        if (!apply_seccomp(SandboxProfile::Default).applied) return int{kNotApplied};
        errno = 0;
        if (::syscall(__NR_memfd_create, "euxis-test", 0) != -1 || errno != EPERM) {
            return int{kNotBlocked};
        }
        return exec_true(); // 0 once sh exits; errno if execve was refused
    });
    EXPECT_EQ(code, kOk);
}

TEST_F(SandboxLinuxTest, StrictProfileBlocksExec) {
    const int code = exit_code_in_child([] {
        if (!apply_seccomp(SandboxProfile::Strict).applied) return int{kNotApplied};
        return exec_true() == EPERM ? int{kOk} : int{kNotBlocked};
    });
    EXPECT_EQ(code, kOk);
}

TEST_F(SandboxLinuxTest, PermissiveProfileSetsNoNewPrivsOnly) {
    const int code = exit_code_in_child([] {
        if (!apply_seccomp(SandboxProfile::Permissive).applied) return int{kNotApplied};
        if (::prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) != 1) return int{kWrongState};
        if (::prctl(PR_GET_SECCOMP) != 0) return int{kWrongState}; // no filter
        return exec_true();
    });
    EXPECT_EQ(code, kOk);
}

TEST_F(SandboxLinuxTest, TestProcessIsNeverFiltered) {
    // Regression guard for issue #96: nothing above leaks into this process.
    EXPECT_EQ(::prctl(PR_GET_SECCOMP), 0);
    EXPECT_EQ(exit_code_in_child(exec_true), kOk);
}

#else // !__linux__

TEST(SandboxTest, NonLinuxReportsUnavailable) {
    EXPECT_FALSE(seccomp_available());
    for (auto profile : {SandboxProfile::Default, SandboxProfile::Permissive,
                         SandboxProfile::Strict}) {
        EXPECT_FALSE(apply_seccomp(profile).applied);
    }
}

#endif

} // namespace euxis::platform
