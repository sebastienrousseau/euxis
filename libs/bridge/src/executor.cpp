#include <euxis/bridge/executor.hpp>
#include <euxis/bridge/platform.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>

#include <spdlog/spdlog.h>

#if defined(__linux__)
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace euxis::bridge {

namespace {

/// P10-R2: Maximum number of poll iterations (bounds the read loop).
constexpr int kMaxPollIterations = 6000;

/// P10-R2: Default poll interval in milliseconds.
constexpr int kPollIntervalMs = 100;

/// P10-R2: Maximum output buffer size (16 MB).
constexpr size_t kMaxOutputBytes = 16ULL * 1024 * 1024;

/// P10-R5: Environment variable names that must never be overridden.
constexpr std::array kDeniedEnvVars = {
    "LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES",
    "DYLD_LIBRARY_PATH", "PATH", "HOME", "USER", "SHELL",
};

/// Check whether an env var name is on the deny list.
[[nodiscard]] auto is_denied_env(const std::string& name) -> bool {
    return std::any_of(kDeniedEnvVars.begin(), kDeniedEnvVars.end(),
                       [&](const char* denied) { return name == denied; });
}

/// Result of a subprocess spawn (shared across platforms).
struct SpawnResult {
    int exit_code{-1};
    std::string stdout_output;
    std::string stderr_output;
    std::chrono::milliseconds duration{0};
};

#ifdef __linux__
auto safe_spawn(const std::vector<std::string>& argv,
                const std::vector<std::string>& env_vars,
                int timeout_seconds = 60)
    -> std::expected<SpawnResult, std::string> {

    assert(!argv.empty() && "P10-R5: argv must contain at least a program name");
    assert(timeout_seconds > 0 && "P10-R5: timeout must be positive");

    int stdout_pipe[2];
    if (pipe(stdout_pipe) != 0) {
        return std::unexpected("pipe() failed: " + std::string(strerror(errno)));
    }

    auto start = std::chrono::steady_clock::now();
    pid_t pid = fork();

    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return std::unexpected("fork() failed: " + std::string(strerror(errno)));
    }

    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);

        // Set environment variables (filtered against deny list)
        for (const auto& e : env_vars) {
            auto eq = e.find('=');
            if (eq != std::string::npos) {
                auto var_name = e.substr(0, eq);
                if (!is_denied_env(var_name)) {
                    setenv(var_name.c_str(), e.substr(eq + 1).c_str(), 1);
                }
            }
        }

        // Build argv for execvp
        std::vector<const char*> c_argv;
        for (const auto& a : argv) {
            c_argv.push_back(a.c_str());
        }
        c_argv.push_back(nullptr);

        execvp(c_argv[0], const_cast<char* const*>(c_argv.data()));
        _exit(127);
    }

    // Parent process
    close(stdout_pipe[1]);

    std::string output;
    output.reserve(64ULL * 1024);  // Pre-allocate 64KB to avoid early reallocations
    std::array<char, 4096> buffer{};

    struct pollfd pfd{};
    pfd.fd = stdout_pipe[0];
    pfd.events = POLLIN;

    // P10-R2: Bounded poll loop — max iterations derived from timeout.
    const int max_iterations = std::min(
        kMaxPollIterations,
        (timeout_seconds * 1000) / kPollIntervalMs);
    int poll_count = 0;

    while (poll_count++ < max_iterations) {
        int ret = poll(&pfd, 1, kPollIntervalMs);
        if (ret <= 0) break;
        ssize_t n = read(stdout_pipe[0], buffer.data(), buffer.size());
        if (n <= 0) break;
        output.append(buffer.data(), static_cast<size_t>(n));
        if (output.size() > kMaxOutputBytes) break;  // P10-R2: output bounded
    }

    assert(poll_count <= max_iterations + 1 && "P10-R2: poll loop terminated within bounds");

    close(stdout_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return SpawnResult{
        .exit_code = exit_code,
        .stdout_output = output,
        .stderr_output = "",
        .duration = duration,
    };
}
#endif

#ifdef __APPLE__
auto safe_spawn_posix(const std::vector<std::string>& argv,
                      const std::vector<std::string>& env_vars,
                      int timeout_seconds = 60)
    -> std::expected<SpawnResult, std::string> {

    assert(!argv.empty() && "argv must contain at least a program name");
    assert(timeout_seconds > 0 && "timeout must be positive");

    int stdout_pipe[2];
    if (pipe(stdout_pipe) != 0) {
        return std::unexpected("pipe() failed: " + std::string(strerror(errno)));
    }

    // Set up file actions: redirect stdout+stderr to pipe
    posix_spawn_file_actions_t actions{};
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addclose(&actions, stdout_pipe[0]);
    posix_spawn_file_actions_adddup2(&actions, stdout_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, stdout_pipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, stdout_pipe[1]);

    // Build C argv
    std::vector<const char*> c_argv;
    for (const auto& a : argv) c_argv.push_back(a.c_str());
    c_argv.push_back(nullptr);

    // Build environment: start from current environ, overlay filtered vars
    std::vector<std::string> env_storage;
    std::vector<const char*> c_env;
    for (char** e = environ; *e; ++e) env_storage.emplace_back(*e);
    for (const auto& e : env_vars) {
        auto eq = e.find('=');
        if (eq != std::string::npos && !is_denied_env(e.substr(0, eq))) {
            env_storage.push_back(e);
        }
    }
    for (const auto& s : env_storage) c_env.push_back(s.c_str());
    c_env.push_back(nullptr);

    auto start = std::chrono::steady_clock::now();
    pid_t pid = 0;
    int rc = posix_spawnp(&pid,
                          c_argv[0],
                          &actions,
                          nullptr,
                          const_cast<char* const*>(c_argv.data()),
                          const_cast<char* const*>(c_env.data()));
    posix_spawn_file_actions_destroy(&actions);

    if (rc != 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return std::unexpected("posix_spawnp() failed: " + std::string(strerror(rc)));
    }

    // Parent: read output via poll loop (same pattern as Linux path)
    close(stdout_pipe[1]);

    std::string output;
    output.reserve(std::size_t{64} * 1024);
    std::array<char, 4096> buffer{};

    struct pollfd pfd{};
    pfd.fd = stdout_pipe[0];
    pfd.events = POLLIN;

    const int max_iterations = std::min(
        kMaxPollIterations,
        (timeout_seconds * 1000) / kPollIntervalMs);
    int poll_count = 0;

    while (poll_count++ < max_iterations) {
        int ret = poll(&pfd, 1, kPollIntervalMs);
        if (ret <= 0) break;
        ssize_t n = read(stdout_pipe[0], buffer.data(), buffer.size());
        if (n <= 0) break;
        output.append(buffer.data(), static_cast<size_t>(n));
        if (output.size() > kMaxOutputBytes) break;
    }

    close(stdout_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return SpawnResult{
        .exit_code = exit_code,
        .stdout_output = output,
        .stderr_output = "",
        .duration = duration,
    };
}
#endif

}  // namespace

SkillExecutor::SkillExecutor(std::optional<SkillExecutionPolicy> policy)
    : policy_(std::move(policy)) {}

auto SkillExecutor::run(const BridgedSkill& skill)
    -> std::expected<ExecutionResult, std::string> {
    assert(!skill.name.empty() && "P10-R5: skill name must not be empty");
    assert(!skill.entrypoint.empty() && "P10-R5: skill entrypoint must be set");

    auto env = build_env(skill);

    auto platform = detect_platform();
    if (platform.has_nsjail && policy_) {
        spdlog::info("Running skill '{}' in nsjail sandbox", skill.name);
        return spawn_sandboxed(skill.entrypoint, env);
    }

    spdlog::info("Running skill '{}' directly", skill.name);
    return spawn_direct(skill.entrypoint, env);
}

auto SkillExecutor::spawn_sandboxed(
    const std::filesystem::path& entrypoint,
    const std::vector<std::string>& env
) -> std::expected<ExecutionResult, std::string> {
#ifdef __linux__
    auto runtime = runtime_command(
        entrypoint.extension().string() == ".py" ? "python" : "node"
    );

    // Build nsjail argv safely (no shell)
    std::vector<std::string> argv = {"nsjail", "--mode", "o"};

    argv.push_back("--time_limit");
    argv.push_back(std::to_string(policy_ ? policy_->resources.timeout_seconds : 20u));

    argv.push_back("--rlimit_as");
    argv.push_back(std::to_string(policy_ ? policy_->resources.memory_mb : 256u));

    argv.push_back("--chroot");
    argv.push_back("/");

    if (policy_ && policy_->filesystem.read_only) {
        argv.push_back("--disable_proc");
    }

    if (policy_ && policy_->network.deny_all) {
        argv.push_back("--disable_clone_newnet");
        argv.push_back("0");
    }

    for (const auto& e : env) {
        argv.push_back("--env");
        argv.push_back(e);
    }

    argv.push_back("--");
    argv.push_back(runtime);
    argv.push_back(entrypoint.string());

    const int timeout = policy_ ? static_cast<int>(policy_->resources.timeout_seconds) : 60;
    auto result = safe_spawn(argv, {}, timeout);
    if (!result) return std::unexpected(result.error());

    return ExecutionResult{
        .exit_code = result->exit_code,
        .stdout_output = result->stdout_output,
        .stderr_output = result->stderr_output,
        .duration = result->duration,
    };
#else
    (void)entrypoint;
    (void)env;
    return std::unexpected("Sandboxed execution requires Linux");
#endif
}

auto SkillExecutor::spawn_direct(
    const std::filesystem::path& entrypoint,
    const std::vector<std::string>& env
) -> std::expected<ExecutionResult, std::string> {
#if defined(__linux__)
    auto runtime = runtime_command(
        entrypoint.extension().string() == ".py" ? "python" : "node"
    );

    std::vector<std::string> argv = {runtime, entrypoint.string()};

    const int timeout = policy_ ? static_cast<int>(policy_->resources.timeout_seconds) : 60;
    auto result = safe_spawn(argv, env, timeout);
    if (!result) return std::unexpected(result.error());

    return ExecutionResult{
        .exit_code = result->exit_code,
        .stdout_output = result->stdout_output,
        .stderr_output = result->stderr_output,
        .duration = result->duration,
    };
#elif defined(__APPLE__)
    auto runtime = runtime_command(
        entrypoint.extension().string() == ".py" ? "python" : "node"
    );

    std::vector<std::string> argv = {runtime, entrypoint.string()};

    const int timeout = policy_ ? static_cast<int>(policy_->resources.timeout_seconds) : 60;
    auto result = safe_spawn_posix(argv, env, timeout);
    if (!result) return std::unexpected(result.error());

    return ExecutionResult{
        .exit_code = result->exit_code,
        .stdout_output = result->stdout_output,
        .stderr_output = result->stderr_output,
        .duration = result->duration,
    };
#else
    (void)entrypoint;
    (void)env;
    return std::unexpected("Direct execution not supported on this platform");
#endif
}

auto SkillExecutor::build_env(const BridgedSkill& skill) const
    -> std::vector<std::string> {
    std::vector<std::string> env;
    env.push_back("EUXIS_SKILL_NAME=" + skill.name);
    env.push_back("EUXIS_SKILL_SLUG=" + skill.slug);
    env.push_back("EUXIS_SKILL_RUNTIME=" + skill.runtime);
    env.push_back("EUXIS_SKILL_DIR=" + skill.source_dir.string());

    if (policy_) {
        env.push_back("EUXIS_TIMEOUT=" + std::to_string(policy_->resources.timeout_seconds));
        env.push_back("EUXIS_MEMORY_MB=" + std::to_string(policy_->resources.memory_mb));
        env.push_back(std::string("EUXIS_NETWORK=") +
                      (policy_->network.deny_all ? "deny" : "allow"));
    }

    return env;
}

auto SkillExecutor::runtime_command(const std::string& runtime) const
    -> std::string {
    if (runtime == "python") return "python3";
    if (runtime == "node") return "node";
    return runtime;
}

}  // namespace euxis::bridge
