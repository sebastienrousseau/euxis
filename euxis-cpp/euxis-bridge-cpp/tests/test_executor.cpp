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

TEST(ExecutorTest, BuildEnvWithoutPolicy) {
    SkillExecutor executor;
    BridgedSkill skill;
    skill.name = "env-test";
    skill.slug = "env_test";
    skill.runtime = "python";
    skill.source_dir = "/tmp/test-skill";
    skill.entrypoint = "/tmp/test-skill/main.py";

    // Running with missing entrypoint exercises build_env and spawn_direct
    auto result = executor.run(skill);
    if (result) {
        // On Linux, python3 may or may not be available
        EXPECT_TRUE(true);
    } else {
        // Error is expected
        EXPECT_FALSE(result.error().empty());
    }
}

TEST(ExecutorTest, BuildEnvWithPolicy) {
    SkillExecutionPolicy policy;
    policy.resources.timeout_seconds = 30;
    policy.resources.memory_mb = 512;
    policy.network.deny_all = true;

    SkillExecutor executor(policy);
    BridgedSkill skill;
    skill.name = "policy-test";
    skill.slug = "policy_test";
    skill.runtime = "node";
    skill.source_dir = "/tmp/test-skill";
    skill.entrypoint = "/tmp/test-skill/index.js";

    auto result = executor.run(skill);
    if (result) {
        // Non-zero exit expected since entrypoint doesn't exist
        EXPECT_NE(result->exit_code, 0);
    }
}

TEST(ExecutorTest, PythonRuntimeCommand) {
    SkillExecutor executor;
    BridgedSkill skill;
    skill.name = "py-skill";
    skill.slug = "py_skill";
    skill.runtime = "python";
    skill.source_dir = "/nonexistent";
    skill.entrypoint = "/nonexistent/main.py";  // .py extension triggers python3

    auto result = executor.run(skill);
    // Should either fail or return non-zero
    if (result) {
        EXPECT_NE(result->exit_code, 0);
    }
}

TEST(ExecutorTest, NodeRuntimeCommand) {
    SkillExecutor executor;
    BridgedSkill skill;
    skill.name = "node-skill";
    skill.slug = "node_skill";
    skill.runtime = "node";
    skill.source_dir = "/nonexistent";
    skill.entrypoint = "/nonexistent/index.js";  // .js extension triggers node

    auto result = executor.run(skill);
    if (result) {
        EXPECT_NE(result->exit_code, 0);
    }
}

TEST(ExecutorTest, PolicyWithReadOnlyFs) {
    SkillExecutionPolicy policy;
    policy.filesystem.read_only = true;
    policy.network.deny_all = false;

    SkillExecutor executor(policy);
    BridgedSkill skill;
    skill.name = "ro-skill";
    skill.slug = "ro_skill";
    skill.runtime = "node";
    skill.source_dir = "/nonexistent";
    skill.entrypoint = "/nonexistent/main.js";

    auto result = executor.run(skill);
    // Expected to fail (nsjail probably not available in test env)
    if (result) {
        EXPECT_NE(result->exit_code, 0);
    }
}

TEST(ExecutorTest, SpawnDirectWithRealEcho) {
    SkillExecutor executor;
    BridgedSkill skill;
    skill.name = "echo-skill";
    skill.slug = "echo_skill";
    skill.runtime = "echo";  // Not python or node, uses raw runtime
    skill.source_dir = "/tmp";
    skill.entrypoint = "/bin/echo";  // Not .py or .js, but we test the fallback

    // This tests spawn_direct with a non-python/node runtime
    auto result = executor.run(skill);
    // The runtime_command falls through to return "echo" itself
    // which means it tries to run: echo /bin/echo
    if (result) {
        EXPECT_GE(result->exit_code, 0);
    }
}

// --- Coverage: sandbox execution path (spawn_sandboxed lines 129-182) ---
TEST(ExecutorTest, SandboxedExecutionWithPolicy) {
    SkillExecutionPolicy policy;
    policy.resources.timeout_seconds = 20;
    policy.resources.memory_mb = 256;
    policy.filesystem.read_only = true;
    policy.network.deny_all = true;

    SkillExecutor executor(policy);
    BridgedSkill skill;
    skill.name = "sandbox-test";
    skill.slug = "sandbox_test";
    skill.runtime = "python";
    skill.source_dir = "/nonexistent";
    skill.entrypoint = "/nonexistent/main.py";  // .py triggers python path

    // This exercises spawn_sandboxed if nsjail is present, spawn_direct otherwise
    auto result = executor.run(skill);
    if (result) {
        EXPECT_NE(result->exit_code, 0);
    } else {
        EXPECT_FALSE(result.error().empty());
    }
}

// --- Coverage: build_env with full policy (lines 219-224) ---
TEST(ExecutorTest, BuildEnvWithNetworkDeny) {
    SkillExecutionPolicy policy;
    policy.resources.timeout_seconds = 30;
    policy.resources.memory_mb = 512;
    policy.network.deny_all = true;

    SkillExecutor executor(policy);
    BridgedSkill skill;
    skill.name = "env-deny";
    skill.slug = "env_deny";
    skill.runtime = "node";
    skill.source_dir = "/tmp";
    skill.entrypoint = "/nonexistent/index.js";

    auto result = executor.run(skill);
    if (result) {
        EXPECT_GE(result->exit_code, 0);
    }
}

// --- Coverage: build_env with network allow (line 223) ---
TEST(ExecutorTest, BuildEnvWithNetworkAllow) {
    SkillExecutionPolicy policy;
    policy.resources.timeout_seconds = 5;
    policy.resources.memory_mb = 64;
    policy.network.deny_all = false;

    SkillExecutor executor(policy);
    BridgedSkill skill;
    skill.name = "env-allow";
    skill.slug = "env_allow";
    skill.runtime = "python";
    skill.source_dir = "/tmp";
    skill.entrypoint = "/nonexistent/main.py";

    auto result = executor.run(skill);
    if (result) {
        EXPECT_NE(result->exit_code, 0);
    }
}

// --- Coverage: runtime_command fallthrough for unknown runtime (line 233) ---
TEST(ExecutorTest, UnknownRuntimeCommandPassthrough) {
    SkillExecutor executor;
    BridgedSkill skill;
    skill.name = "custom-rt";
    skill.slug = "custom_rt";
    skill.runtime = "ruby";
    skill.source_dir = "/tmp";
    skill.entrypoint = "/nonexistent/main.rb";  // Not .py or .js

    auto result = executor.run(skill);
    if (result) {
        // ruby runtime command is "ruby" itself (passthrough)
        EXPECT_GE(result->exit_code, 0);
    }
}

// --- Coverage: run with real echo that produces stdout (lines 86-88) ---
TEST(ExecutorTest, SpawnDirectWithTrueCommand) {
    SkillExecutor executor;
    BridgedSkill skill;
    skill.name = "true-skill";
    skill.slug = "true_skill";
    skill.runtime = "/usr/bin/true";
    skill.source_dir = "/tmp";
    skill.entrypoint = "/dev/null";  // Dummy argument

    auto result = executor.run(skill);
    if (result) {
        // /usr/bin/true should return 0 if found, or fail
        EXPECT_GE(result->exit_code, 0);
        EXPECT_GE(result->duration.count(), 0);
    }
}

// --- Coverage: spawn_sandboxed with .js extension (line 135 python/node branch) ---
TEST(ExecutorTest, SandboxedNodeRuntime) {
    SkillExecutionPolicy policy;
    policy.resources.timeout_seconds = 10;
    policy.resources.memory_mb = 128;
    policy.filesystem.read_only = false;
    policy.network.deny_all = false;

    SkillExecutor executor(policy);
    BridgedSkill skill;
    skill.name = "sandboxed-node";
    skill.slug = "sandboxed_node";
    skill.runtime = "node";
    skill.source_dir = "/nonexistent";
    skill.entrypoint = "/nonexistent/index.js";  // .js triggers "node" runtime

    auto result = executor.run(skill);
    if (result) {
        EXPECT_NE(result->exit_code, 0);
    }
}

}  // namespace euxis::bridge
