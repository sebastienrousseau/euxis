#include "euxis/cli/process.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include <functional>

namespace euxis::cli {
namespace {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

/// Remaining milliseconds until deadline, clamped to [0, INT_MAX].
auto ms_remaining(TimePoint deadline) -> int {
    auto now = Clock::now();
    if (now >= deadline) return 0;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    return static_cast<int>(std::min<long long>(ms, 2'000'000'000LL));
}

/// Apply resource limits to the child process to prevent OOM (137)
void apply_resource_limits() {
    // 2026-03-13: Decommissioned setrlimit calls. 
    // Node.js/V8 initialization is incompatible with RLIMIT_DATA/AS in containerized or mise environments.
    // We rely on host-level cgroups and the OS OOM killer for safety.
}

/// Read all data from a file descriptor, respecting a deadline.
auto read_fd(int fd, TimePoint deadline) -> std::string {
    std::string result;
    std::array<char, 4096> buf{};
    for (;;) {
        int timeout_ms = ms_remaining(deadline);
        if (timeout_ms <= 0) break;

        struct pollfd pfd{fd, POLLIN, 0};
        int pr = ::poll(&pfd, 1, timeout_ms);
        if (pr <= 0) break;  // timeout or error

        ssize_t n = ::read(fd, buf.data(), buf.size());
        if (n <= 0) break;   // EOF or error
        result.append(buf.data(), static_cast<size_t>(n));
    }
    return result;
}

/// Wait for child process to exit, kill it if deadline is exceeded.
auto reap_child(pid_t pid, TimePoint deadline) -> int {
    for (;;) {
        int status = 0;
        pid_t w = ::waitpid(pid, &status, WNOHANG);
        if (w > 0) return status;
        if (w < 0) return -1;

        if (Clock::now() >= deadline) {
            // Graceful shutdown first
            ::kill(pid, SIGTERM);
            ::usleep(200'000);  // 200ms grace
            if (::waitpid(pid, &status, WNOHANG) > 0) return status;
            // Force kill
            ::kill(pid, SIGKILL);
            ::waitpid(pid, &status, 0);
            return status;
        }
        ::usleep(20'000);  // 20ms poll interval
    }
}

} // namespace

auto Process::run(const std::string& program,
                  const std::vector<std::string>& args,
                  int timeout_seconds) -> ProcessResult {
    int stdout_pipe[2];
    int stderr_pipe[2];
    if (::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0) {
        return {-1, "", "pipe() failed"};
    }

    std::vector<const char*> argv;
    argv.push_back(program.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdout_pipe[0]); ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]); ::close(stderr_pipe[1]);
        return {-1, "", "fork() failed"};
    }

    if (pid == 0) {
        // Child
        apply_resource_limits();
        ::close(stdout_pipe[0]);
        ::close(stderr_pipe[0]);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);

        ::execvp(program.c_str(), const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    // Parent
    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);

    auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds);

    ProcessResult result;
    result.stdout_output = read_fd(stdout_pipe[0], deadline);
    result.stderr_output = read_fd(stderr_pipe[0], deadline);
    ::close(stdout_pipe[0]);
    ::close(stderr_pipe[0]);

    int status = reap_child(pid, deadline);
    if (status == -1) {
        result.exit_code = -1;
        result.stderr_output += "\n[timeout after " + std::to_string(timeout_seconds) + "s]";
    } else if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }

    return result;
}

auto Process::run_with_input(const std::string& program,
                              const std::vector<std::string>& args,
                              const std::string& stdin_data,
                              int timeout_seconds) -> ProcessResult {
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];
    if (::pipe(stdin_pipe) != 0 || ::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0) {
        return {-1, "", "pipe() failed"};
    }

    std::vector<const char*> argv;
    argv.push_back(program.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdin_pipe[0]); ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]); ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]); ::close(stderr_pipe[1]);
        return {-1, "", "fork() failed"};
    }

    if (pid == 0) {
        // Child
        apply_resource_limits();
        ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]);
        ::close(stderr_pipe[0]);
        ::dup2(stdin_pipe[0], STDIN_FILENO);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdin_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);

        ::execvp(program.c_str(), const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    // Parent
    ::close(stdin_pipe[0]);
    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);

    // Write stdin data then close to signal EOF
    if (!stdin_data.empty()) {
        const char* data = stdin_data.data();
        size_t remaining = stdin_data.size();
        while (remaining > 0) {
            auto written = ::write(stdin_pipe[1], data, remaining);
            if (written <= 0) break;
            data += written;
            remaining -= static_cast<size_t>(written);
        }
    }
    ::close(stdin_pipe[1]);

    auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds);

    ProcessResult result;
    result.stdout_output = read_fd(stdout_pipe[0], deadline);
    result.stderr_output = read_fd(stderr_pipe[0], deadline);
    ::close(stdout_pipe[0]);
    ::close(stderr_pipe[0]);

    int status = reap_child(pid, deadline);
    if (status == -1) {
        result.exit_code = -1;
        result.stderr_output += "\n[timeout after " + std::to_string(timeout_seconds) + "s]";
    } else if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }

    return result;
}

auto Process::run_streaming(const std::string& program,
                            const std::vector<std::string>& args,
                            const std::string& stdin_data,
                            std::function<void(const std::string&)> on_chunk,
                            int timeout_seconds) -> ProcessResult {
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];
    if (::pipe(stdin_pipe) != 0 || ::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0) {
        return {-1, "", "pipe() failed"};
    }

    std::vector<const char*> argv;
    argv.push_back(program.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdin_pipe[0]); ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]); ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]); ::close(stderr_pipe[1]);
        return {-1, "", "fork() failed"};
    }

    if (pid == 0) {
        // Child
        apply_resource_limits();
        ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]);
        ::close(stderr_pipe[0]);
        ::dup2(stdin_pipe[0], STDIN_FILENO);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdin_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);

        ::execvp(program.c_str(), const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    // Parent
    ::close(stdin_pipe[0]);
    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);

    if (!stdin_data.empty()) {
        const char* data = stdin_data.data();
        size_t remaining = stdin_data.size();
        while (remaining > 0) {
            auto written = ::write(stdin_pipe[1], data, remaining);
            if (written <= 0) break;
            data += written;
            remaining -= static_cast<size_t>(written);
        }
    }
    ::close(stdin_pipe[1]);

    auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds);
    ProcessResult result;
    
    // Concurrent read using poll
    bool stdout_open = true;
    bool stderr_open = true;
    while (stdout_open || stderr_open) {
        int timeout_ms = ms_remaining(deadline);
        if (timeout_ms <= 0) break;

        std::array<pollfd, 2> pfds{};
        int nfds = 0;
        if (stdout_open) pfds[nfds++] = {stdout_pipe[0], POLLIN, 0};
        if (stderr_open) pfds[nfds++] = {stderr_pipe[0], POLLIN, 0};

        if (nfds == 0) break;

        int pr = ::poll(pfds.data(), nfds, timeout_ms);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (pr == 0) break; // Timeout

        for (int i = 0; i < nfds; ++i) {
            if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                std::array<char, 4096> buf{};
                ssize_t n = ::read(pfds[i].fd, buf.data(), buf.size());
                if (n > 0) {
                    std::string chunk(buf.data(), static_cast<size_t>(n));
                    if (pfds[i].fd == stdout_pipe[0]) {
                        result.stdout_output += chunk;
                        if (on_chunk) on_chunk(chunk);
                    } else {
                        result.stderr_output += chunk;
                    }
                } else {
                    if (pfds[i].fd == stdout_pipe[0]) stdout_open = false;
                    else stderr_open = false;
                }
            }
        }
    }

    ::close(stdout_pipe[0]);
    ::close(stderr_pipe[0]);

    int status = reap_child(pid, deadline);
    if (status == -1) {
        result.exit_code = -1;
        result.stderr_output += "\n[timeout after " + std::to_string(timeout_seconds) + "s]";
    } else if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }

    return result;
}

auto Process::shell(const std::string& command, int timeout_seconds) -> ProcessResult {
    return run("/bin/sh", {"-c", command}, timeout_seconds);
}

auto Process::shell_interactive(const std::string& command) -> int {
    // system() connects the command to the parent's stdin/stdout/stderr
    int status = std::system(command.c_str());
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

auto Process::which(const std::string& name) -> std::optional<std::string> {
    const char* path_env = std::getenv("PATH");
    if (!path_env) return std::nullopt;

    std::string path_str(path_env);
    size_t start = 0;
    while (start < path_str.size()) {
        size_t end = path_str.find(':', start);
        if (end == std::string::npos) end = path_str.size();

        auto dir = path_str.substr(start, end - start);
        auto candidate = std::filesystem::path(dir) / name;
        if (std::filesystem::exists(candidate) &&
            ::access(candidate.c_str(), X_OK) == 0) {
            return candidate.string();
        }
        start = end + 1;
    }
    return std::nullopt;
}

auto Process::available(const std::string& name) -> bool {
    return which(name).has_value();
}

} // namespace euxis::cli
