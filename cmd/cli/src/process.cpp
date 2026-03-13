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

auto ms_remaining(TimePoint deadline) -> int {
    auto now = Clock::now();
    if (now >= deadline) return 0;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
    return static_cast<int>(std::min<long long>(ms, 2'000'000'000LL));
}

[[maybe_unused]] void apply_resource_limits() {}

void apply_env_vars(const std::map<std::string, std::string>& env_vars) {
    for (const auto& [key, value] : env_vars) {
        ::setenv(key.c_str(), value.c_str(), 1);
    }
}

auto read_fd(int fd, TimePoint deadline) -> std::string {
    std::string result;
    std::array<char, 8192> buf{}; // Increased buffer size
    for (;;) {
        int timeout_ms = ms_remaining(deadline);
        if (timeout_ms <= 0) break;

        struct pollfd pfd{fd, POLLIN, 0};
        int pr = ::poll(&pfd, 1, timeout_ms);
        if (pr <= 0) break;

        ssize_t n = ::read(fd, buf.data(), buf.size());
        if (n <= 0) break;
        result.append(buf.data(), static_cast<size_t>(n));
    }
    return result;
}

/// Robustly reap child and capture final output buffers.
auto reap_child(pid_t pid, TimePoint deadline, int out_fd = -1, int err_fd = -1, std::string* out_str = nullptr, std::string* err_str = nullptr) -> int {
    int status = 0;
    for (;;) {
        pid_t w = ::waitpid(pid, &status, WNOHANG);
        if (w > 0) return status;
        if (w < 0) return -1;

        if (Clock::now() >= deadline) {
            ::kill(pid, SIGTERM);
            // "Last Breath" capture: process may emit logs while shutting down
            auto shutdown_deadline = Clock::now() + std::chrono::milliseconds(500);
            if (out_fd != -1 && out_str) out_str->append(read_fd(out_fd, shutdown_deadline));
            if (err_fd != -1 && err_str) err_str->append(read_fd(err_fd, shutdown_deadline));

            if (::waitpid(pid, &status, WNOHANG) > 0) return status;
            ::kill(pid, SIGKILL);
            ::waitpid(pid, &status, 0);
            return status;
        }
        ::usleep(50'000);
    }
}

} // namespace

auto Process::run(const std::string& program,
                  const std::vector<std::string>& args,
                  int timeout_seconds,
                  const std::map<std::string, std::string>& env_vars) -> ProcessResult {
    int stdout_pipe[2];
    int stderr_pipe[2];
    if (::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0) return {-1, "", "pipe() failed"};

    std::vector<const char*> argv;
    argv.push_back(program.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdout_pipe[0]); ::close(stdout_pipe[1]); ::close(stderr_pipe[0]); ::close(stderr_pipe[1]);
        return {-1, "", "fork() failed"};
    }

    if (pid == 0) {
        apply_env_vars(env_vars);
        ::close(stdout_pipe[0]); ::close(stderr_pipe[0]);
        ::dup2(stdout_pipe[1], STDOUT_FILENO); ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdout_pipe[1]); ::close(stderr_pipe[1]);
        ::execvp(program.c_str(), const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    ::close(stdout_pipe[1]); ::close(stderr_pipe[1]);
    auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds);

    ProcessResult result;
    result.stdout_output = read_fd(stdout_pipe[0], deadline);
    result.stderr_output = read_fd(stderr_pipe[0], deadline);

    result.exit_code = reap_child(pid, deadline, stdout_pipe[0], stderr_pipe[0], &result.stdout_output, &result.stderr_output);
    
    ::close(stdout_pipe[0]); ::close(stderr_pipe[0]);
    if (WIFEXITED(result.exit_code)) result.exit_code = WEXITSTATUS(result.exit_code);
    else if (WIFSIGNALED(result.exit_code)) result.exit_code = 128 + WTERMSIG(result.exit_code);

    return result;
}

auto Process::run_with_input(const std::string& program,
                              const std::vector<std::string>& args,
                              const std::string& stdin_data,
                              int timeout_seconds,
                              const std::map<std::string, std::string>& env_vars) -> ProcessResult {
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    if (::pipe(stdin_pipe) != 0 || ::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0) return {-1, "", "pipe() failed"};

    std::vector<const char*> argv;
    argv.push_back(program.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdin_pipe[0]); ::close(stdin_pipe[1]); ::close(stdout_pipe[0]); ::close(stdout_pipe[1]); ::close(stderr_pipe[0]); ::close(stderr_pipe[1]);
        return {-1, "", "fork() failed"};
    }

    if (pid == 0) {
        apply_env_vars(env_vars);
        ::close(stdin_pipe[1]); ::close(stdout_pipe[0]); ::close(stderr_pipe[0]);
        ::dup2(stdin_pipe[0], STDIN_FILENO); ::dup2(stdout_pipe[1], STDOUT_FILENO); ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdin_pipe[0]); ::close(stdout_pipe[1]); ::close(stderr_pipe[1]);
        ::execvp(program.c_str(), const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    ::close(stdin_pipe[0]); ::close(stdout_pipe[1]); ::close(stderr_pipe[1]);
    if (!stdin_data.empty()) [[maybe_unused]] auto _ = ::write(stdin_pipe[1], stdin_data.data(), stdin_data.size());
    ::close(stdin_pipe[1]);

    auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds);
    ProcessResult result;
    result.stdout_output = read_fd(stdout_pipe[0], deadline);
    result.stderr_output = read_fd(stderr_pipe[0], deadline);

    result.exit_code = reap_child(pid, deadline, stdout_pipe[0], stderr_pipe[0], &result.stdout_output, &result.stderr_output);
    
    ::close(stdout_pipe[0]); ::close(stderr_pipe[0]);
    if (WIFEXITED(result.exit_code)) result.exit_code = WEXITSTATUS(result.exit_code);
    else if (WIFSIGNALED(result.exit_code)) result.exit_code = 128 + WTERMSIG(result.exit_code);

    return result;
}

auto Process::run_streaming(const std::string& program,
                            const std::vector<std::string>& args,
                            const std::string& stdin_data,
                            std::function<void(const std::string&)> on_chunk,
                            int timeout_seconds,
                            const std::map<std::string, std::string>& env_vars) -> ProcessResult {
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    if (::pipe(stdin_pipe) != 0 || ::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0) return {-1, "", "pipe() failed"};

    std::vector<const char*> argv;
    argv.push_back(program.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdin_pipe[0]); ::close(stdin_pipe[1]); ::close(stdout_pipe[0]); ::close(stdout_pipe[1]); ::close(stderr_pipe[0]); ::close(stderr_pipe[1]);
        return {-1, "", "fork() failed"};
    }

    if (pid == 0) {
        apply_env_vars(env_vars);
        ::close(stdin_pipe[1]); ::close(stdout_pipe[0]); ::close(stderr_pipe[0]);
        ::dup2(stdin_pipe[0], STDIN_FILENO); ::dup2(stdout_pipe[1], STDOUT_FILENO); ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdin_pipe[0]); ::close(stdout_pipe[1]); ::close(stderr_pipe[1]);
        ::execvp(program.c_str(), const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    ::close(stdin_pipe[0]); ::close(stdout_pipe[1]); ::close(stderr_pipe[1]);
    if (!stdin_data.empty()) [[maybe_unused]] auto _ = ::write(stdin_pipe[1], stdin_data.data(), stdin_data.size());
    ::close(stdin_pipe[1]);

    auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds);
    ProcessResult result;
    bool stdout_open = true, stderr_open = true;
    while (stdout_open || stderr_open) {
        int timeout_ms = ms_remaining(deadline);
        if (timeout_ms <= 0) break;
        struct pollfd pfds[2]{{stdout_pipe[0], POLLIN, 0}, {stderr_pipe[0], POLLIN, 0}};
        if (::poll(pfds, 2, timeout_ms) <= 0) break;
        for (int i = 0; i < 2; ++i) {
            if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                std::array<char, 8192> buf{};
                ssize_t n = ::read(pfds[i].fd, buf.data(), buf.size());
                if (n > 0) {
                    std::string chunk(buf.data(), static_cast<size_t>(n));
                    if (pfds[i].fd == stdout_pipe[0]) { result.stdout_output += chunk; if (on_chunk) on_chunk(chunk); }
                    else result.stderr_output += chunk;
                } else { if (i == 0) stdout_open = false; else stderr_open = false; }
            }
        }
    }

    result.exit_code = reap_child(pid, deadline, stdout_pipe[0], stderr_pipe[0], &result.stdout_output, &result.stderr_output);
    ::close(stdout_pipe[0]); ::close(stderr_pipe[0]);
    if (WIFEXITED(result.exit_code)) result.exit_code = WEXITSTATUS(result.exit_code);
    else if (WIFSIGNALED(result.exit_code)) result.exit_code = 128 + WTERMSIG(result.exit_code);
    return result;
}

auto Process::shell(const std::string& command, int timeout_seconds) -> ProcessResult { return run("/bin/sh", {"-c", command}, timeout_seconds); }
auto Process::shell_interactive(const std::string& command) -> int { int status = std::system(command.c_str()); return WIFEXITED(status) ? WEXITSTATUS(status) : -1; }
auto Process::which(const std::string& name) -> std::optional<std::string> {
    const char* path_env = std::getenv("PATH"); if (!path_env) return std::nullopt;
    std::string path_str(path_env); size_t start = 0;
    while (start < path_str.size()) {
        size_t end = path_str.find(':', start); if (end == std::string::npos) end = path_str.size();
        auto dir = path_str.substr(start, end - start); auto candidate = std::filesystem::path(dir) / name;
        if (std::filesystem::exists(candidate) && ::access(candidate.c_str(), X_OK) == 0) return candidate.string();
        start = end + 1;
    }
    return std::nullopt;
}
auto Process::available(const std::string& name) -> bool { return which(name).has_value(); }

} // namespace euxis::cli
