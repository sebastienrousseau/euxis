#include "euxis/cli/process.hpp"

#include <array>
#include <cstring>
#include <filesystem>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace euxis::cli {
namespace {

/// Read all data from a file descriptor into a string.
auto read_fd(int fd) -> std::string {
    std::string result;
    std::array<char, 4096> buf{};
    ssize_t n;
    while ((n = ::read(fd, buf.data(), buf.size())) > 0) {
        result.append(buf.data(), static_cast<size_t>(n));
    }
    return result;
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

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdout_pipe[0]); ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]); ::close(stderr_pipe[1]);
        return {-1, "", "fork() failed"};
    }

    if (pid == 0) {
        // Child
        ::close(stdout_pipe[0]);
        ::close(stderr_pipe[0]);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);

        std::vector<const char*> argv;
        argv.push_back(program.c_str());
        for (const auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);

        ::execvp(program.c_str(), const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    // Parent
    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);

    // Set timeout alarm
    if (timeout_seconds > 0) {
        ::alarm(static_cast<unsigned>(timeout_seconds));
    }

    ProcessResult result;
    result.stdout_output = read_fd(stdout_pipe[0]);
    result.stderr_output = read_fd(stderr_pipe[0]);
    ::close(stdout_pipe[0]);
    ::close(stderr_pipe[0]);

    int status = 0;
    ::waitpid(pid, &status, 0);
    ::alarm(0);

    if (WIFEXITED(status)) {
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

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdin_pipe[0]); ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]); ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]); ::close(stderr_pipe[1]);
        return {-1, "", "fork() failed"};
    }

    if (pid == 0) {
        // Child
        ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]);
        ::close(stderr_pipe[0]);
        ::dup2(stdin_pipe[0], STDIN_FILENO);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdin_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);

        std::vector<const char*> argv;
        argv.push_back(program.c_str());
        for (const auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);

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

    if (timeout_seconds > 0) {
        ::alarm(static_cast<unsigned>(timeout_seconds));
    }

    ProcessResult result;
    result.stdout_output = read_fd(stdout_pipe[0]);
    result.stderr_output = read_fd(stderr_pipe[0]);
    ::close(stdout_pipe[0]);
    ::close(stderr_pipe[0]);

    int status = 0;
    ::waitpid(pid, &status, 0);
    ::alarm(0);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }

    return result;
}

auto Process::shell(const std::string& command, int timeout_seconds) -> ProcessResult {
    return run("/bin/sh", {"-c", command}, timeout_seconds);
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
