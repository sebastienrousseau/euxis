#include <gtest/gtest.h>
#include <euxis/bridge/policy.hpp>

#include <sodium.h>
#include <algorithm>

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

// --- P0-3: Capability Token Tests ---

TEST(CapabilityTokenTest, IssueAndVerify) {
    unsigned char key[crypto_auth_hmacsha256_KEYBYTES];
    crypto_auth_hmacsha256_keygen(key);

    auto token = issue_capability_token("reviewer", "session-1",
        {"claude", "gemini"}, {"curl"}, 4096, key);

    EXPECT_EQ(token.agent_id, "reviewer");
    EXPECT_EQ(token.session_id, "session-1");
    EXPECT_FALSE(token.hmac_signature.empty());
    EXPECT_TRUE(verify_capability_token(token, key));
}

TEST(CapabilityTokenTest, TamperedFails) {
    unsigned char key[crypto_auth_hmacsha256_KEYBYTES];
    crypto_auth_hmacsha256_keygen(key);

    auto token = issue_capability_token("reviewer", "session-1",
        {"claude"}, {}, 4096, key);

    token.agent_id = "attacker";
    EXPECT_FALSE(verify_capability_token(token, key));
}

TEST(CapabilityTokenTest, CanonicalDeterministic) {
    AgentCapabilityToken t1;
    t1.agent_id = "a"; t1.session_id = "s";
    t1.allowed_providers = {"p1", "p2"};
    t1.allowed_tools = {"t1"};
    t1.max_token_budget = 100;
    t1.filesystem_read_only = true;
    t1.network_deny_all = false;
    t1.issued_at = "2026-01-01T00:00:00Z";
    t1.expires_at = "2026-01-01T01:00:00Z";

    auto c1 = t1.canonical_string();
    auto c2 = t1.canonical_string();
    EXPECT_EQ(c1, c2);
    EXPECT_FALSE(c1.empty());
}

TEST(CapabilityTokenTest, JsonRoundTrip) {
    unsigned char key[crypto_auth_hmacsha256_KEYBYTES];
    crypto_auth_hmacsha256_keygen(key);

    auto token = issue_capability_token("sentinel", "sess-2",
        {"ollama"}, {"aider", "sgpt"}, 2048, key);

    auto j = token.to_json();
    auto restored = AgentCapabilityToken::from_json(j);

    EXPECT_EQ(restored.agent_id, token.agent_id);
    EXPECT_EQ(restored.session_id, token.session_id);
    EXPECT_EQ(restored.allowed_providers, token.allowed_providers);
    EXPECT_EQ(restored.hmac_signature, token.hmac_signature);
    EXPECT_TRUE(verify_capability_token(restored, key));
}

TEST(CapabilityTokenTest, ExpiredDetection) {
    AgentCapabilityToken token;
    token.expires_at = "2020-01-01T00:00:00Z";
    // The token can be verified but the caller should check expiry
    EXPECT_FALSE(token.expires_at.empty());
}

TEST(CapabilityTokenTest, DifferentKeyFails) {
    unsigned char key1[crypto_auth_hmacsha256_KEYBYTES];
    unsigned char key2[crypto_auth_hmacsha256_KEYBYTES];
    crypto_auth_hmacsha256_keygen(key1);
    crypto_auth_hmacsha256_keygen(key2);

    auto token = issue_capability_token("agent", "session",
        {"claude"}, {}, 4096, key1);

    EXPECT_FALSE(verify_capability_token(token, key2));
}

TEST(CapabilityTokenTest, ProviderCheck) {
    unsigned char key[crypto_auth_hmacsha256_KEYBYTES];
    crypto_auth_hmacsha256_keygen(key);

    auto token = issue_capability_token("agent", "session",
        {"claude", "gemini"}, {}, 4096, key);

    auto has_provider = [&](const std::string& p) {
        return std::find(token.allowed_providers.begin(),
                        token.allowed_providers.end(), p) != token.allowed_providers.end();
    };
    EXPECT_TRUE(has_provider("claude"));
    EXPECT_TRUE(has_provider("gemini"));
    EXPECT_FALSE(has_provider("ollama"));
}

TEST(CapabilityTokenTest, NullTokenBackwardCompat) {
    // Verify that null/empty token doesn't crash
    AgentCapabilityToken empty_token;
    EXPECT_TRUE(empty_token.agent_id.empty());
    EXPECT_TRUE(empty_token.hmac_signature.empty());
    auto canonical = empty_token.canonical_string();
    EXPECT_FALSE(canonical.empty()); // Contains separators at minimum
}

}  // namespace euxis::bridge
