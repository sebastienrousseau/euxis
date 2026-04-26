#include "euxis/platform/sandbox.hpp"

#ifdef __linux__

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <stddef.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>

namespace euxis::platform {

namespace {
/// Detect WSL2 via /proc/version — WSL2 has incomplete seccomp BPF support
/// and filter installation may fail silently or return EINVAL.
auto is_wsl() -> bool {
    std::ifstream f("/proc/version");
    if (!f.is_open()) return false;
    std::string line;
    std::getline(f, line);
    // Both WSL1 and WSL2 contain "microsoft" (case-insensitive) in /proc/version
    return line.find("microsoft") != std::string::npos
        || line.find("Microsoft") != std::string::npos;
}
} // namespace

auto seccomp_available() -> bool {
    if (is_wsl()) return false;
    // PR_GET_SECCOMP returns 0 if seccomp is available (mode disabled),
    // or the current mode if already active.  Returns -1 with EINVAL
    // if the kernel was built without CONFIG_SECCOMP.
    int ret = ::prctl(PR_GET_SECCOMP);
    return ret >= 0;
}

auto apply_seccomp(SandboxProfile profile) -> SandboxResult {
    SandboxResult result;

    // WSL2 has incomplete seccomp support — gracefully degrade to no-filter.
    if (is_wsl()) {
        result.applied = false;
        result.error = "seccomp BPF skipped on WSL (incomplete kernel support)";
        return result;
    }

    // Step 1: Set no-new-privileges (required for unprivileged seccomp filters).
    if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        result.error = std::string("prctl(PR_SET_NO_NEW_PRIVS) failed: ") + std::strerror(errno);
        return result;
    }

    // Permissive profile: just no_new_privs, no BPF filter.
    if (profile == SandboxProfile::Permissive) {
        result.applied = true;
        return result;
    }

    // Build the BPF filter program.
    //
    // Structure:
    //   [0] Load syscall number (seccomp_data.nr at offset 0)
    //   [1..N] JEQ checks — if syscall matches a blocked nr, jump to DENY
    //   [N+1] ALLOW (default action)
    //   [N+2] DENY  (return EPERM)

    // Blocked syscalls for Default profile.
    // S9: Added execveat, memfd_create, userfaultfd to prevent container/sandbox escape
    int blocked_default[] = {
        __NR_mount,
        __NR_umount2,
        __NR_ptrace,
        __NR_reboot,
        __NR_swapon,
        __NR_swapoff,
        __NR_init_module,
        __NR_pivot_root,
        __NR_kexec_load,
        __NR_execveat,
        __NR_memfd_create,
        __NR_userfaultfd,
    };
    constexpr int n_default = sizeof(blocked_default) / sizeof(blocked_default[0]);

    // Strict profile adds __NR_execve.
    int blocked_strict[] = {
        __NR_mount,
        __NR_umount2,
        __NR_ptrace,
        __NR_reboot,
        __NR_swapon,
        __NR_swapoff,
        __NR_init_module,
        __NR_pivot_root,
        __NR_kexec_load,
        __NR_execve,
        __NR_execveat,
        __NR_memfd_create,
        __NR_userfaultfd,
    };
    constexpr int n_strict = sizeof(blocked_strict) / sizeof(blocked_strict[0]);

    int* blocked = (profile == SandboxProfile::Strict) ? blocked_strict : blocked_default;
    int n_blocked = (profile == SandboxProfile::Strict) ? n_strict : n_default;

    // Total instructions: 1 (load) + n_blocked (JEQ checks) + 1 (ALLOW) + 1 (DENY)
    int total = 1 + n_blocked + 2;

    // S8: RAII allocation — no leak on early return or exception
    auto filter = std::make_unique<struct sock_filter[]>(total);
    int idx = 0;

    // [0] BPF_LD | BPF_W | BPF_ABS — load seccomp_data.nr (syscall number)
    filter[idx++] = BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                             offsetof(struct seccomp_data, nr));

    // [1..N] For each blocked syscall: if nr == blocked[i], jump to DENY.
    // Jump offsets: jt = distance to DENY instruction, jf = 0 (fall through).
    for (int i = 0; i < n_blocked; ++i) {
        // Distance from current instruction to DENY:
        //   remaining JEQ checks: (n_blocked - 1 - i)
        //   + 1 for the ALLOW instruction
        unsigned char jt = static_cast<unsigned char>((n_blocked - 1 - i) + 1);
        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
                                 static_cast<unsigned int>(blocked[i]),
                                 jt, 0);
    }

    // [N+1] ALLOW — default action for non-blocked syscalls.
    filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);

    // [N+2] DENY — return EPERM for blocked syscalls.
    filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA));

    struct sock_fprog prog {};
    prog.len = static_cast<unsigned short>(total);
    prog.filter = filter.get();

    if (::prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        result.error = std::string("prctl(PR_SET_SECCOMP) failed: ") + std::strerror(errno);
        return result;  // S8: unique_ptr auto-frees
    }

    result.applied = true;
    return result;
}

} // namespace euxis::platform

#else // !__linux__

namespace euxis::platform {

auto seccomp_available() -> bool { return false; }

auto apply_seccomp(SandboxProfile /*profile*/) -> SandboxResult {
    return {false, "seccomp is only available on Linux"};
}

} // namespace euxis::platform

#endif
