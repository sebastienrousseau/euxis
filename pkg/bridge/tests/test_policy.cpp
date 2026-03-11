#include <gtest/gtest.h>
#include <euxis/bridge/policy.hpp>

namespace euxis::bridge {

TEST(PolicyTest, DefaultValues) {
    SkillExecutionPolicy policy;
    EXPECT_FALSE(policy.require_signature);
    EXPECT_TRUE(policy.filesystem.read_only);
    EXPECT_TRUE(policy.network.deny_all);
    EXPECT_EQ(policy.resources.memory_mb, 256u);
    EXPECT_EQ(policy.resources.timeout_seconds, 20u);
}

TEST(PolicyTest, Permissive) {
    auto policy = SkillExecutionPolicy::permissive();
    EXPECT_FALSE(policy.require_signature);
    EXPECT_FALSE(policy.filesystem.read_only);
    EXPECT_FALSE(policy.network.deny_all);
    EXPECT_EQ(policy.resources.memory_mb, 512u);
    EXPECT_EQ(policy.resources.timeout_seconds, 60u);
}

TEST(PolicyTest, ToJson) {
    SkillExecutionPolicy policy;
    policy.require_signature = true;
    policy.network.allowed_hosts = {"example.com"};

    auto j = policy.to_json();
    EXPECT_TRUE(j["require_signature"]);
    EXPECT_TRUE(j["filesystem"]["read_only"]);
    EXPECT_TRUE(j["network"]["deny_all"]);
    EXPECT_EQ(j["network"]["allowed_hosts"][0], "example.com");
}

TEST(PolicyTest, ResourceLimits) {
    ResourceLimits limits;
    limits.memory_mb = 1024;
    limits.timeout_seconds = 120;
    EXPECT_EQ(limits.memory_mb, 1024u);
    EXPECT_EQ(limits.timeout_seconds, 120u);
}

TEST(PolicyTest, FilesystemPolicy) {
    FilesystemPolicy fs;
    fs.read_only = false;
    fs.write_paths = {"/tmp", "/var/data"};
    EXPECT_FALSE(fs.read_only);
    EXPECT_EQ(fs.write_paths.size(), 2);
}

// --- Coverage: line 19 (to_json with public_key_path set) ---
TEST(PolicyTest, ToJsonWithPublicKeyPath) {
    SkillExecutionPolicy policy;
    policy.public_key_path = "/tmp/pubkey.pem";

    auto j = policy.to_json();
    EXPECT_TRUE(j.contains("public_key_path"));
    EXPECT_EQ(j["public_key_path"], "/tmp/pubkey.pem");
}

// --- Coverage: line 24 (to_json with write_paths) ---
TEST(PolicyTest, ToJsonWithWritePaths) {
    SkillExecutionPolicy policy;
    policy.filesystem.read_only = false;
    policy.filesystem.write_paths = {"/tmp", "/var/data"};

    auto j = policy.to_json();
    EXPECT_EQ(j["filesystem"]["write_paths"].size(), 2u);
    EXPECT_EQ(j["filesystem"]["write_paths"][0], "/tmp");
}

// --- Coverage: line 32 (to_json with output_schema set) ---
TEST(PolicyTest, ToJsonWithOutputSchema) {
    SkillExecutionPolicy policy;
    policy.output_schema = nlohmann::json{{"type", "object"}};

    auto j = policy.to_json();
    EXPECT_TRUE(j.contains("output_schema"));
    EXPECT_EQ(j["output_schema"]["type"], "object");
}

// --- Coverage: line 35 (to_json with audit_log_path set) ---
TEST(PolicyTest, ToJsonWithAuditLogPath) {
    SkillExecutionPolicy policy;
    policy.audit_log_path = "/var/log/audit.jsonl";

    auto j = policy.to_json();
    EXPECT_TRUE(j.contains("audit_log_path"));
    EXPECT_EQ(j["audit_log_path"], "/var/log/audit.jsonl");
}

}  // namespace euxis::bridge
