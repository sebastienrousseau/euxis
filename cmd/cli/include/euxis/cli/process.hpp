#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace euxis::cli {

/// Result of running a child process.
struct ProcessResult {
    int exit_code{-1};
    std::string stdout_output;
    std::string stderr_output;
};

/// Spawn processes and capture output.
class Process {
public:
    /// Run a command with arguments. Captures stdout/stderr via pipes.
    static auto run(const std::string& program,
                    const std::vector<std::string>& args,
                    int timeout_seconds = 30) -> ProcessResult;

    /// Run a command with stdin data piped to the child process.
    static auto run_with_input(const std::string& program,
                               const std::vector<std::string>& args,
                               const std::string& stdin_data,
                               int timeout_seconds = 120) -> ProcessResult;

    /// Run a command and stream stdout back via a callback in real-time.
    static auto run_streaming(const std::string& program,
                              const std::vector<std::string>& args,
                              const std::string& stdin_data,
                              std::function<void(const std::string&)> on_chunk,
                              int timeout_seconds = 120) -> ProcessResult;

    /// Run a shell command string (via /bin/sh -c).
    static auto shell(const std::string& command,
                      int timeout_seconds = 30) -> ProcessResult;

    /// Run a shell command string interactively (connects to parent terminal).
    /// Returns 0 on success, or the exit code.
    static auto shell_interactive(const std::string& command) -> int;

    /// Search PATH for an executable. Returns the full path or nullopt.
    static auto which(const std::string& name) -> std::optional<std::string>;

    /// Check if a program is available on PATH.
    static auto available(const std::string& name) -> bool;
};

} // namespace euxis::cli
