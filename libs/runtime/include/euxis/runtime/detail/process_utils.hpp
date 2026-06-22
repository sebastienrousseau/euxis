#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <cstdio>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace euxis::runtime::detail {

// Execute a command without shell interpretation (safe from injection)
inline bool exec_no_shell(const std::string& program, const std::vector<std::string>& args) {
#if defined(__unix__) || defined(__APPLE__)
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // Redirect stdout/stderr to /dev/null
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); close(devnull); }
        std::vector<const char*> argv;
        argv.push_back(program.c_str());
        for (const auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);
        execvp(program.c_str(), const_cast<char* const*>(argv.data()));
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
    (void)program; (void)args;
    return false;
#endif
}

// Internal helper for capturing command output
inline std::string run_command(const std::string& cmd) {
    std::array<char, 4096> buffer{};
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

// Internal helper for piping to a command's stdin
inline bool run_pipe_command(const std::string& cmd, std::string_view input) {
    FILE* pipe = popen(cmd.c_str(), "w");
    if (!pipe) return false;
    const auto written = fwrite(input.data(), 1, input.size(), pipe);
    const bool full_write = written == input.size();
    return pclose(pipe) == 0 && full_write;
}

} // namespace euxis::runtime::detail
