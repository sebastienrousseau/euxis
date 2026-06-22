/// @file
/// @brief Executor for running bridged skills in a controlled environment.
#pragma once

#include <chrono>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "skill.hpp"
#include "policy.hpp"

namespace euxis::bridge {

/// @brief Captured output and status from a skill execution.
struct ExecutionResult {
    int exit_code{0};
    std::string stdout_output;
    std::string stderr_output;
    std::chrono::milliseconds duration{0};
};

/// @brief Handles the spawning and lifecycle of a skill process.
class SkillExecutor {
public:
    /// @brief Construct executor with an optional security policy.
    explicit SkillExecutor(std::optional<SkillExecutionPolicy> policy = std::nullopt);

    /// @brief Run a skill and capture its output.
    auto run(const BridgedSkill& skill) -> std::expected<ExecutionResult, std::string>;

private:
    std::optional<SkillExecutionPolicy> policy_;

    auto spawn_sandboxed(const std::filesystem::path& entrypoint,
                         const std::vector<std::string>& env) -> std::expected<ExecutionResult, std::string>;
    
    auto spawn_direct(const std::filesystem::path& entrypoint,
                      const std::vector<std::string>& env) -> std::expected<ExecutionResult, std::string>;

    auto build_env(const BridgedSkill& skill) const -> std::vector<std::string>;
    auto runtime_command(const std::string& runtime) const -> std::string;
};

} // namespace euxis::bridge
