/// @file
/// @brief Implementation of the local + Docker execution backends.

#include "euxis/platform/execution_backend.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#if defined(__linux__)
#  include <sys/prctl.h>
#endif

namespace euxis::platform {

namespace {

constexpr std::size_t kReadChunk = 4096;

// Convert std::vector<std::string> argv into a NULL-terminated array of
// C strings suitable for execvp. Storage is borrowed from @p argv.
[[nodiscard]] auto to_execvp_argv(const std::vector<std::string>& argv)
    -> std::vector<char*> {
    std::vector<char*> out;
    out.reserve(argv.size() + 1);
    for (const auto& s : argv) {
        out.push_back(const_cast<char*>(s.c_str()));
    }
    out.push_back(nullptr);
    return out;
}

// Set the close-on-exec flag on @p fd; logged-but-ignored failures.
void set_cloexec(int fd) noexcept {
    const int flags = ::fcntl(fd, F_GETFD, 0);
    if (flags >= 0) {
        (void)::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
}

// Drain both stdout + stderr concurrently, respecting one deadline.
struct DrainResult {
    bool eof_stdout{false};
    bool eof_stderr{false};
    bool deadline_hit{false};
};

[[nodiscard]] auto drain_pair(int fd_out, int fd_err,
                               std::string& s_out, std::string& s_err,
                               std::chrono::steady_clock::time_point deadline)
    -> DrainResult {
    DrainResult res;
    while (!res.eof_stdout || !res.eof_stderr) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) { res.deadline_hit = true; return res; }
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);

        struct pollfd pfds[2];
        nfds_t n = 0;
        if (!res.eof_stdout) {
            pfds[n].fd = fd_out;
            pfds[n].events = POLLIN;
            pfds[n].revents = 0;
            ++n;
        }
        if (!res.eof_stderr) {
            pfds[n].fd = fd_err;
            pfds[n].events = POLLIN;
            pfds[n].revents = 0;
            ++n;
        }
        if (n == 0) break;

        const int prc = ::poll(pfds, n, static_cast<int>(remaining.count()));
        if (prc < 0) {
            if (errno == EINTR) continue;
            res.deadline_hit = true; return res;
        }
        if (prc == 0) { res.deadline_hit = true; return res; }

        // Process each pollfd that signalled.
        for (nfds_t i = 0; i < n; ++i) {
            if ((pfds[i].revents & (POLLIN | POLLHUP)) == 0) continue;
            std::array<char, kReadChunk> buf{};
            const ssize_t r = ::read(pfds[i].fd, buf.data(), buf.size());
            const bool is_out = (pfds[i].fd == fd_out);
            if (r == 0) {
                if (is_out) res.eof_stdout = true;
                else        res.eof_stderr = true;
            } else if (r > 0) {
                std::string& sink = is_out ? s_out : s_err;
                sink.append(buf.data(), static_cast<std::size_t>(r));
            } else {
                if (errno == EINTR) continue;
                if (is_out) res.eof_stdout = true;
                else        res.eof_stderr = true;
            }
        }
    }
    return res;
}

// Run a single argv with the given options. Used by both LocalBackend
// and the helper in DockerBackend::is_available().
[[nodiscard]] auto run_local(const ExecutionRequest& req) -> ExecutionResult {
    ExecutionResult res;
    res.backend_name = "local";

    if (req.argv.empty()) {
        res.error = "argv must not be empty";
        return res;
    }

    int out_pipe[2]   = {-1, -1};
    int err_pipe[2]   = {-1, -1};
    int stdin_pipe[2] = {-1, -1};
    if (::pipe(out_pipe) != 0 || ::pipe(err_pipe) != 0 || ::pipe(stdin_pipe) != 0) {
        res.error = std::string{"pipe() failed: "} + std::strerror(errno);
        return res;
    }
    set_cloexec(out_pipe[0]);
    set_cloexec(err_pipe[0]);
    set_cloexec(stdin_pipe[1]);

    const auto t0 = std::chrono::steady_clock::now();
    const pid_t pid = ::fork();
    if (pid < 0) {
        res.error = std::string{"fork() failed: "} + std::strerror(errno);
        ::close(out_pipe[0]); ::close(out_pipe[1]);
        ::close(err_pipe[0]); ::close(err_pipe[1]);
        ::close(stdin_pipe[0]); ::close(stdin_pipe[1]);
        return res;
    }

    if (pid == 0) {
        // CHILD. Wire the pipes and exec.
        ::dup2(stdin_pipe[0], STDIN_FILENO);
        ::dup2(out_pipe[1],   STDOUT_FILENO);
        ::dup2(err_pipe[1],   STDERR_FILENO);
        ::close(stdin_pipe[0]); ::close(stdin_pipe[1]);
        ::close(out_pipe[0]);   ::close(out_pipe[1]);
        ::close(err_pipe[0]);   ::close(err_pipe[1]);

#if defined(__linux__)
        // Restore PR_SET_DUMPABLE in case the parent was ASan-instrumented
        // and zeroed it. No-op otherwise. See issue #96.
        (void)::prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
#endif

        if (req.working_dir) {
            if (::chdir(req.working_dir->c_str()) != 0) {
                std::fprintf(stderr, "chdir failed: %s\n", std::strerror(errno));
                _exit(127);
            }
        }
        for (const auto& [k, v] : req.env) {
            ::setenv(k.c_str(), v.c_str(), /*overwrite=*/1);
        }

        // Resolve argv[0] against PATH ourselves rather than relying on
        // execvp's libc-internal resolver. GitHub Actions' ubuntu-24.04
        // runner has been observed returning EPERM from execvp("echo")
        // even when /usr/bin/echo is present, executable, and ACL-clean
        // (issue #96 round-6 confirmed dumpable=1, AppArmor relaxed).
        // Calling execv() with the resolved absolute path skips
        // the libc PATH search and the kernel boundary check that
        // triggers the EPERM. Falls through to the original execvp
        // behaviour if the argv[0] already contains a '/' or PATH is
        // unset.
        auto argv_c = to_execvp_argv(req.argv);
        std::string resolved;
        if (argv_c[0] != nullptr && std::strchr(argv_c[0], '/') == nullptr) {
            const char* path_env = std::getenv("PATH");
            if (path_env != nullptr) {
                std::string_view path_view{path_env};
                while (!path_view.empty()) {
                    const auto colon = path_view.find(':');
                    const auto dir   = path_view.substr(0,
                        colon == std::string_view::npos ? path_view.size() : colon);
                    std::string candidate{dir};
                    if (!candidate.empty() && candidate.back() != '/') {
                        candidate.push_back('/');
                    }
                    candidate.append(argv_c[0]);
                    if (::access(candidate.c_str(), X_OK) == 0) {
                        resolved = std::move(candidate);
                        break;
                    }
                    if (colon == std::string_view::npos) break;
                    path_view.remove_prefix(colon + 1);
                }
            }
        }
        if (!resolved.empty()) {
            ::execv(resolved.c_str(), argv_c.data());
        } else {
            ::execvp(argv_c[0], argv_c.data());
        }
        // exec failed — diagnose and exit with the canonical "127".
        // Prefix "execvp failed" is load-bearing: a test in this suite
        // asserts on that exact phrase (NonexistentBinaryProducesNonZero
        // ExitCode → stderr.find("execvp failed")). Keep it even though
        // we may have used execv() on a resolved path.
        const int exec_errno = errno;
        std::fprintf(stderr,
                     "execvp failed: %s (errno=%d argv[0]=%s resolved=%s)\n",
                     std::strerror(exec_errno), exec_errno, argv_c[0],
                     resolved.empty() ? "<unresolved>" : resolved.c_str());
        _exit(127);
    }

    // PARENT.
    ::close(stdin_pipe[0]);
    ::close(out_pipe[1]);
    ::close(err_pipe[1]);

    // Write stdin (if any) then close the write end.
    if (req.stdin_text) {
        const std::string& s = *req.stdin_text;
        std::size_t written = 0;
        while (written < s.size()) {
            const ssize_t w =
                ::write(stdin_pipe[1], s.data() + written, s.size() - written);
            if (w < 0) {
                if (errno == EINTR) continue;
                break;
            }
            written += static_cast<std::size_t>(w);
        }
    }
    ::close(stdin_pipe[1]);

    const auto deadline = t0 + req.timeout;
    auto drain = drain_pair(out_pipe[0], err_pipe[0],
                            res.stdout_text, res.stderr_text, deadline);
    ::close(out_pipe[0]);
    ::close(err_pipe[0]);

    if (drain.deadline_hit) {
        // Kill the child group; reap it; report timeout.
        ::kill(pid, SIGKILL);
        (void)::waitpid(pid, nullptr, 0);
        const auto t1 = std::chrono::steady_clock::now();
        res.wall_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
        res.error = "timeout";
        return res;
    }

    int status = 0;
    while (true) {
        const pid_t w = ::waitpid(pid, &status, 0);
        if (w >= 0) break;
        if (errno == EINTR) continue;
        res.error = std::string{"waitpid failed: "} + std::strerror(errno);
        return res;
    }
    const auto t1 = std::chrono::steady_clock::now();
    res.wall_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    if (WIFEXITED(status)) {
        res.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        res.exit_code = 128 + WTERMSIG(status);
        res.error = std::string{"killed by signal "} +
                    std::to_string(WTERMSIG(status));
    } else {
        res.exit_code = -1;
        res.error = "abnormal termination";
    }
    return res;
}

} // namespace

// ---------------------------------------------------------------------------
// LocalBackend
// ---------------------------------------------------------------------------

auto LocalBackend::name() const noexcept -> std::string_view { return "local"; }

auto LocalBackend::execute(const ExecutionRequest& req) -> ExecutionResult {
    return run_local(req);
}

// ---------------------------------------------------------------------------
// DockerBackend
// ---------------------------------------------------------------------------

auto DockerBackend::name() const noexcept -> std::string_view { return "docker"; }

auto DockerBackend::is_available() noexcept -> bool {
    ExecutionRequest probe;
    probe.argv = {"docker", "info"};
    probe.timeout = std::chrono::milliseconds{2000};
    auto r = run_local(probe);
    if (r.error.has_value()) return false;
    return r.exit_code == 0;
}

auto DockerBackend::execute(const ExecutionRequest& req) -> ExecutionResult {
    if (!is_available()) {
        ExecutionResult res;
        res.backend_name = "docker";
        res.error = "docker not available on PATH";
        return res;
    }
    if (req.argv.empty()) {
        ExecutionResult res;
        res.backend_name = "docker";
        res.error = "argv must not be empty";
        return res;
    }

    // Build the docker run argv.
    ExecutionRequest sub;
    sub.argv = {"docker", "run", "--rm", "-i"};
    if (req.working_dir) {
        sub.argv.push_back("-w");
        sub.argv.push_back(req.working_dir->string());
    }
    for (const auto& [k, v] : req.env) {
        sub.argv.push_back("-e");
        sub.argv.push_back(k + "=" + v);
    }
    for (const auto& [host, ctr] : req.docker_read_mounts) {
        sub.argv.push_back("-v");
        sub.argv.push_back(host.string() + ":" + ctr + ":ro");
    }
    for (const auto& [host, ctr] : req.docker_write_mounts) {
        sub.argv.push_back("-v");
        sub.argv.push_back(host.string() + ":" + ctr);
    }
    sub.argv.push_back(req.docker_image);
    for (const auto& a : req.argv) sub.argv.push_back(a);

    // Inherit timeout + stdin from the caller; we pipe everything through
    // run_local() which already implements deadline + drain.
    sub.timeout    = req.timeout;
    sub.stdin_text = req.stdin_text;

    auto r = run_local(sub);
    r.backend_name = "docker";  // override the inner "local" tag
    return r;
}

} // namespace euxis::platform
