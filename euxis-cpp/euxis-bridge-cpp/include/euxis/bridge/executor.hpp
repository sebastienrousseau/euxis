#pragma once

#include <chrono>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "policy.hpp"
#include "skill.hpp"

namespace euxis::bridge {

struct ExecutionResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
    std::chrono::milliseconds duration;
};

class SkillExecutor {
public:
    explicit SkillExecutor(std::optional<SkillExecutionPolicy> policy = std::nullopt);

    [[nodiscard]] auto run(const BridgedSkill& skill)
        -> std::expected<ExecutionResult, std::string>;

private:
    std::optional<SkillExecutionPolicy> policy_;

    [[nodiscard]] auto spawn_sandboxed(
        const std::filesystem::path& entrypoint,
        const std::vector<std::string>& env
    ) -> std::expected<ExecutionResult, std::string>;

    [[nodiscard]] auto spawn_direct(
        const std::filesystem::path& entrypoint,
        const std::vector<std::string>& env
    ) -> std::expected<ExecutionResult, std::string>;

    [[nodiscard]] auto build_env(const BridgedSkill& skill) const
        -> std::vector<std::string>;

    [[nodiscard]] auto runtime_command(const std::string& runtime) const
        -> std::string;
};

}  // namespace euxis::bridge
