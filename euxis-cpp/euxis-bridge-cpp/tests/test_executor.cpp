#include <gtest/gtest.h>
#include <euxis/bridge/executor.hpp>

namespace euxis::bridge {

TEST(ExecutorTest, DefaultConstructor) {
    SkillExecutor executor;
    // Should construct without error
}

TEST(ExecutorTest, WithPolicy) {
    SkillExecutionPolicy policy;
    policy.resources.timeout_seconds = 10;
    policy.resources.memory_mb = 128;

    SkillExecutor executor(policy);
    // Should construct without error
}

TEST(ExecutorTest, ExecutionResultFields) {
    ExecutionResult result;
    result.exit_code = 0;
    result.stdout_output = "hello";
    result.stderr_output = "";
    result.duration = std::chrono::milliseconds{100};

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, "hello");
    EXPECT_EQ(result.duration.count(), 100);
}

TEST(ExecutorTest, RunMissingEntrypoint) {
    SkillExecutor executor;

    BridgedSkill skill;
    skill.name = "missing";
    skill.slug = "missing";
    skill.source_dir = "/nonexistent";
    skill.runtime = "node";
    skill.entrypoint = "/nonexistent/index.js";

    auto result = executor.run(skill);
    // Should either return error or non-zero exit code
    if (result) {
        EXPECT_NE(result->exit_code, 0);
    }
}

TEST(ExecutorTest, PermissivePolicy) {
    auto policy = SkillExecutionPolicy::permissive();
    SkillExecutor executor(policy);
    // Should construct without error
}

}  // namespace euxis::bridge
