#include <euxis/bridge/executor.hpp>
#include <euxis/bridge/platform.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>

#include <spdlog/spdlog.h>

#ifdef __linux__
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace euxis::bridge {

SkillExecutor::SkillExecutor(std::optional<SkillExecutionPolicy> policy)
    : policy_(std::move(policy)) {}

auto SkillExecutor::run(const BridgedSkill& skill)
    -> std::expected<ExecutionResult, std::string> {
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
    // Build nsjail command
    std::ostringstream cmd;
    cmd << "nsjail --mode o --time_limit "
        << (policy_ ? policy_->resources.timeout_seconds : 20u)
        << " --rlimit_as "
        << (policy_ ? policy_->resources.memory_mb : 256u)
        << " --chroot / ";

    if (policy_ && policy_->filesystem.read_only) {
        cmd << "--disable_proc ";
    }

    if (policy_ && policy_->network.deny_all) {
        cmd << "--disable_clone_newnet 0 ";
    }

    // Add environment variables
    for (const auto& e : env) {
        cmd << "--env " << e << " ";
    }

    auto runtime = runtime_command(entrypoint.extension().string() == ".py" ? "python" : "node");
    cmd << "-- " << runtime << " " << entrypoint.string() << " 2>&1";

    auto start = std::chrono::steady_clock::now();

    // Execute via popen
    std::array<char, 4096> buffer{};
    std::string output;

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        return std::unexpected("Failed to launch nsjail");
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    int status = pclose(pipe);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    int exit_code = 0;
#ifdef __linux__
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else {
        exit_code = -1;
    }
#else
    exit_code = status;
#endif

    return ExecutionResult{
        .exit_code = exit_code,
        .stdout_output = output,
        .stderr_output = "",
        .duration = duration,
    };
}

auto SkillExecutor::spawn_direct(
    const std::filesystem::path& entrypoint,
    const std::vector<std::string>& env
) -> std::expected<ExecutionResult, std::string> {
    auto runtime = runtime_command(
        entrypoint.extension().string() == ".py" ? "python" : "node"
    );

    std::ostringstream cmd;
    for (const auto& e : env) {
        cmd << e << " ";
    }
    cmd << runtime << " " << entrypoint.string() << " 2>&1";

    auto start = std::chrono::steady_clock::now();

    std::array<char, 4096> buffer{};
    std::string output;

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        return std::unexpected("Failed to launch process");
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    int status = pclose(pipe);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    int exit_code = 0;
#ifdef __linux__
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else {
        exit_code = -1;
    }
#else
    exit_code = status;
#endif

    return ExecutionResult{
        .exit_code = exit_code,
        .stdout_output = output,
        .stderr_output = "",
        .duration = duration,
    };
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
