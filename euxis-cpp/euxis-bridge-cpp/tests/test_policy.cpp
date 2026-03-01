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

}  // namespace euxis::bridge
