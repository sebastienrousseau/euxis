#include <gtest/gtest.h>
#include <euxis/platform/sandbox.hpp>

#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <fstream>
#include <string>
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

/// Seccomp filters installed on this process ("Seccomp_filters" in
/// /proc/self/status, Linux 5.9+), or -1 if the kernel does not report it.
/// Containers (Docker, podman) start processes with a filter already, so
/// the tests compare against a baseline, never against zero.
[[nodiscard]] auto seccomp_filter_count() -> int {
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);) {
        if (line.rfind("Seccomp_filters:", 0) == 0) return std::stoi(line.substr(16));
    }
    return -1;
}

/// Filters present when the test binary loaded, before any test ran.
const int kBaselineFilters = seccomp_filter_count();
const int kBaselineMode = ::prctl(PR_GET_SECCOMP);

/// True if nothing added a filter since the binary loaded.
[[nodiscard]] auto no_filter_added() -> bool {
    return kBaselineFilters >= 0 ? seccomp_filter_count() == kBaselineFilters
                                 : ::prctl(PR_GET_SECCOMP) == kBaselineMode;
}

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
        if (!no_filter_added()) return int{kWrongState}; // no BPF filter
        return exec_true();
    });
    EXPECT_EQ(code, kOk);
}

TEST_F(SandboxLinuxTest, TestProcessIsNeverFiltered) {
    // Regression guard for issue #96: no test installs a filter here.
    EXPECT_TRUE(no_filter_added());
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
