#include "euxis/cli/auth_profile_store.hpp"

#include <filesystem>
#include <fstream>
#include <thread>

#include <gtest/gtest.h>

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

using namespace euxis::cli;

class AuthProfileStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "euxis_auth_test";
        std::filesystem::create_directories(test_dir_ / "config");
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
};

TEST_F(AuthProfileStoreTest, AddApiKey) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-test-123", "default");

    auto profiles = store.profiles_for("openai");
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles[0].provider, "openai");
    EXPECT_EQ(profiles[0].api_key, "sk-test-123");
    EXPECT_EQ(profiles[0].type, ProfileType::ApiKey);
}

TEST_F(AuthProfileStoreTest, AddOAuth) {
    AuthProfileStore store(test_dir_.string());
    store.add_oauth("anthropic", "sk-ant-oat01-xyz", "sk-ant-ort01-abc",
                    1772415771003, "user@example.com");

    auto profiles = store.profiles_for("anthropic");
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles[0].type, ProfileType::OAuth);
    EXPECT_EQ(profiles[0].access_token, "sk-ant-oat01-xyz");
    EXPECT_EQ(profiles[0].refresh_token, "sk-ant-ort01-abc");
    EXPECT_EQ(profiles[0].email, "user@example.com");
    EXPECT_EQ(profiles[0].expires_at, 1772415771003);
}

TEST_F(AuthProfileStoreTest, RemoveProfile) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-test-123", "default");

    auto profiles = store.profiles_for("openai");
    ASSERT_EQ(profiles.size(), 1);

    store.remove_profile(profiles[0].id);
    EXPECT_TRUE(store.profiles_for("openai").empty());
}

TEST_F(AuthProfileStoreTest, ResolveApiKey) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-test-123");

    auto auth = store.resolve("openai");
    ASSERT_TRUE(auth.has_value());
    EXPECT_EQ(auth->token, "sk-test-123");
    EXPECT_EQ(auth->provider, "openai");
    EXPECT_FALSE(auth->is_oauth);
}

TEST_F(AuthProfileStoreTest, ResolveOAuth) {
    AuthProfileStore store(test_dir_.string());
    // Use a far-future expiry
    store.add_oauth("anthropic", "sk-ant-oat01-xyz", "sk-ant-ort01-abc",
                    9999999999999, "user@example.com");

    auto auth = store.resolve("anthropic");
    ASSERT_TRUE(auth.has_value());
    EXPECT_EQ(auth->token, "sk-ant-oat01-xyz");
    EXPECT_TRUE(auth->is_oauth);
}

TEST_F(AuthProfileStoreTest, RotationOAuthFirst) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("anthropic", "sk-key-123", "api");
    store.add_oauth("anthropic", "sk-ant-oat01-xyz", "sk-ant-ort01-abc",
                    9999999999999, "user@example.com");

    auto auth = store.resolve("anthropic");
    ASSERT_TRUE(auth.has_value());
    // OAuth should be preferred over ApiKey
    EXPECT_TRUE(auth->is_oauth);
    EXPECT_EQ(auth->token, "sk-ant-oat01-xyz");
}

TEST_F(AuthProfileStoreTest, RotationOldestFirst) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-key-1", "first");
    store.add_api_key("openai", "sk-key-2", "second");

    // First resolve should pick first (both last_used_at=0, insertion order)
    auto auth1 = store.resolve("openai");
    ASSERT_TRUE(auth1.has_value());

    // Mark first as used
    store.report_success(auth1->profile_id);

    // Second resolve should pick second (least recently used)
    auto auth2 = store.resolve("openai");
    ASSERT_TRUE(auth2.has_value());
    EXPECT_EQ(auth2->token, "sk-key-2");
}

TEST_F(AuthProfileStoreTest, SessionPinning) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-key-1", "first");
    store.add_api_key("openai", "sk-key-2", "second");

    // Mark first as used so rotation would pick second
    auto auth = store.resolve("openai");
    store.report_success(auth->profile_id);

    // Pin to first
    store.pin_session(auth->profile_id);

    auto pinned = store.resolve("openai");
    ASSERT_TRUE(pinned.has_value());
    EXPECT_EQ(pinned->profile_id, auth->profile_id);

    // Clear pin
    store.clear_session_pin();
}

TEST_F(AuthProfileStoreTest, CooldownEscalation) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-key-1", "test");
    store.add_api_key("openai", "sk-key-2", "backup");

    auto auth1 = store.resolve("openai");
    ASSERT_TRUE(auth1.has_value());
    std::string first_id = auth1->profile_id;

    // Report failure - should trigger cooldown
    store.report_failure(first_id, CooldownReason::RateLimit);

    // Next resolve should skip the cooled-down profile
    auto auth2 = store.resolve("openai");
    ASSERT_TRUE(auth2.has_value());
    EXPECT_NE(auth2->profile_id, first_id);

    // Cooldown remaining should be > 0
    EXPECT_GT(store.cooldown_remaining_ms(first_id), 0);
}

TEST_F(AuthProfileStoreTest, BillingCooldownLonger) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-key-1", "test");
    store.add_api_key("openai", "sk-key-2", "backup");

    auto auth = store.resolve("openai");
    store.report_failure(auth->profile_id, CooldownReason::BillingError);

    // Billing cooldown should be much longer (5h base)
    auto remaining = store.cooldown_remaining_ms(auth->profile_id);
    EXPECT_GT(remaining, 60 * 60 * 1000);  // > 1 hour
}

TEST_F(AuthProfileStoreTest, SuccessResetsCooldown) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-key-1");

    auto auth = store.resolve("openai");
    store.report_failure(auth->profile_id, CooldownReason::RateLimit);
    EXPECT_GT(store.cooldown_remaining_ms(auth->profile_id), 0);

    store.report_success(auth->profile_id);
    EXPECT_EQ(store.cooldown_remaining_ms(auth->profile_id), 0);
}

TEST_F(AuthProfileStoreTest, FallbackChains) {
    AuthProfileStore store(test_dir_.string());
    // Only add a gemini profile, not anthropic
    store.add_api_key("gemini", "gem-key-1");

    // Resolve with fallback — anthropic has no profiles, should fall back
    auto auth = store.resolve_with_fallback("anthropic");
    ASSERT_TRUE(auth.has_value());
    EXPECT_EQ(auth->provider, "gemini");
}

TEST_F(AuthProfileStoreTest, AllExhaustedReturnsNullopt) {
    AuthProfileStore store(test_dir_.string());
    // No profiles at all
    auto auth = store.resolve_with_fallback("anthropic");
    EXPECT_FALSE(auth.has_value());
}

TEST_F(AuthProfileStoreTest, JsonRoundTrip) {
    {
        AuthProfileStore store(test_dir_.string());
        store.add_api_key("openai", "sk-key-1", "test");
        store.add_oauth("anthropic", "sk-ant-oat01-xyz", "sk-ant-ort01-abc",
                        9999999999999, "user@example.com");
        store.set_fallback_chain("anthropic", {"gemini", "ollama"});
        store.save();
    }

    // Reload in new instance
    {
        AuthProfileStore store(test_dir_.string());
        auto openai = store.profiles_for("openai");
        ASSERT_EQ(openai.size(), 1);
        EXPECT_EQ(openai[0].api_key, "sk-key-1");

        auto anthropic = store.profiles_for("anthropic");
        ASSERT_EQ(anthropic.size(), 1);
        EXPECT_EQ(anthropic[0].access_token, "sk-ant-oat01-xyz");

        auto chain = store.fallback_chain("anthropic");
        ASSERT_EQ(chain.size(), 2);
        EXPECT_EQ(chain[0], "gemini");
    }
}

TEST_F(AuthProfileStoreTest, ResolveNoProfilesReturnsNullopt) {
    AuthProfileStore store(test_dir_.string());
    auto auth = store.resolve("nonexistent");
    EXPECT_FALSE(auth.has_value());
}

TEST_F(AuthProfileStoreTest, ExpiredOAuthSkipped) {
    AuthProfileStore store(test_dir_.string());
    // Add with past expiry
    store.add_oauth("anthropic", "sk-ant-oat01-old", "sk-ant-ort01-old",
                    1000, "expired@example.com");  // expired long ago

    auto auth = store.resolve("anthropic");
    EXPECT_FALSE(auth.has_value());
}

TEST_F(AuthProfileStoreTest, DuplicateApiKeyUpdates) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-old", "test");
    store.add_api_key("openai", "sk-new", "test");

    auto profiles = store.profiles_for("openai");
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles[0].api_key, "sk-new");
}

TEST_F(AuthProfileStoreTest, DuplicateOAuthUpdates) {
    AuthProfileStore store(test_dir_.string());
    store.add_oauth("anthropic", "old-token", "old-refresh",
                    9999999999999, "user@test.com");
    store.add_oauth("anthropic", "new-token", "new-refresh",
                    8888888888888, "user@test.com");

    auto profiles = store.profiles_for("anthropic");
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles[0].access_token, "new-token");
    EXPECT_EQ(profiles[0].refresh_token, "new-refresh");
    EXPECT_EQ(profiles[0].expires_at, 8888888888888);
}

TEST_F(AuthProfileStoreTest, AllProfiles) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-1");
    store.add_api_key("anthropic", "sk-2");

    auto all = store.all_profiles();
    EXPECT_EQ(all.size(), 2u);
}

TEST_F(AuthProfileStoreTest, RemoveProfileClearsCooldown) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-1", "test");

    auto auth = store.resolve("openai");
    ASSERT_TRUE(auth.has_value());

    store.report_failure(auth->profile_id, CooldownReason::RateLimit);
    EXPECT_GT(store.cooldown_remaining_ms(auth->profile_id), 0);

    store.remove_profile(auth->profile_id);
    EXPECT_EQ(store.cooldown_remaining_ms(auth->profile_id), 0);
}

TEST_F(AuthProfileStoreTest, RemoveProfileClearsSessionPin) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-1", "test");

    auto auth = store.resolve("openai");
    ASSERT_TRUE(auth.has_value());

    store.pin_session(auth->profile_id);
    store.remove_profile(auth->profile_id);

    // After removing the pinned profile, resolve should return nullopt
    auto resolved = store.resolve("openai");
    EXPECT_FALSE(resolved.has_value());
}

TEST_F(AuthProfileStoreTest, FallbackChainAccess) {
    AuthProfileStore store(test_dir_.string());
    auto chain = store.fallback_chain("anthropic");
    // Default chains are populated
    EXPECT_FALSE(chain.empty());
}

TEST_F(AuthProfileStoreTest, FallbackChainUnknownProvider) {
    AuthProfileStore store(test_dir_.string());
    auto chain = store.fallback_chain("unknown-provider");
    EXPECT_TRUE(chain.empty());
}

TEST_F(AuthProfileStoreTest, SetFallbackChain) {
    AuthProfileStore store(test_dir_.string());
    store.set_fallback_chain("custom", {"a", "b", "c"});
    auto chain = store.fallback_chain("custom");
    ASSERT_EQ(chain.size(), 3u);
    EXPECT_EQ(chain[0], "a");
}

TEST_F(AuthProfileStoreTest, CooldownRemainingNoEntry) {
    AuthProfileStore store(test_dir_.string());
    EXPECT_EQ(store.cooldown_remaining_ms("nonexistent"), 0);
}

TEST_F(AuthProfileStoreTest, BillingCooldownEscalation) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-1", "test");
    store.add_api_key("openai", "sk-2", "backup");

    auto auth = store.resolve("openai");
    ASSERT_TRUE(auth.has_value());

    // First billing failure
    store.report_failure(auth->profile_id, CooldownReason::BillingError);
    auto remaining1 = store.cooldown_remaining_ms(auth->profile_id);

    // Second billing failure should have longer cooldown
    store.report_failure(auth->profile_id, CooldownReason::BillingError);
    auto remaining2 = store.cooldown_remaining_ms(auth->profile_id);
    EXPECT_GT(remaining2, remaining1);
}

TEST_F(AuthProfileStoreTest, AuthErrorCooldown) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-1", "test");
    store.add_api_key("openai", "sk-2", "backup");

    auto auth = store.resolve("openai");
    store.report_failure(auth->profile_id, CooldownReason::AuthError);
    EXPECT_GT(store.cooldown_remaining_ms(auth->profile_id), 0);
}

TEST_F(AuthProfileStoreTest, AddApiKeyWithEmptyLabel) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-1");  // No label

    auto profiles = store.profiles_for("openai");
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles[0].label, "API Key");
    EXPECT_EQ(profiles[0].source, "manual");
}

TEST_F(AuthProfileStoreTest, AddOAuthWithEmptyEmail) {
    AuthProfileStore store(test_dir_.string());
    store.add_oauth("anthropic", "token", "refresh", 9999999999999);

    auto profiles = store.profiles_for("anthropic");
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles[0].label, "OAuth");
}

TEST_F(AuthProfileStoreTest, ToJsonExcludesEnvProfiles) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-1", "manual-key");
    store.import_env_vars();  // May or may not add env profiles

    store.save();

    // Reload and check that env profiles are not persisted
    AuthProfileStore store2(test_dir_.string());
    auto all = store2.all_profiles();
    for (const auto& p : all) {
        EXPECT_NE(p.source, "env");
    }
}

TEST_F(AuthProfileStoreTest, JsonRoundTripWithCooldowns) {
    {
        AuthProfileStore store(test_dir_.string());
        store.add_api_key("openai", "sk-1", "test");
        store.add_api_key("openai", "sk-2", "backup");
        auto auth = store.resolve("openai");
        store.report_failure(auth->profile_id, CooldownReason::RateLimit);
        store.save();
    }
    {
        AuthProfileStore store(test_dir_.string());
        auto profiles = store.profiles_for("openai");
        EXPECT_GE(profiles.size(), 1u);
    }
}

TEST_F(AuthProfileStoreTest, ResolveWithFallbackPrimaryFirst) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("anthropic", "sk-ant-1");
    store.add_api_key("openai", "sk-oai-1");

    // Should prefer primary (anthropic)
    auto auth = store.resolve_with_fallback("anthropic");
    ASSERT_TRUE(auth.has_value());
    EXPECT_EQ(auth->provider, "anthropic");
}

TEST_F(AuthProfileStoreTest, ImportEnvVars) {
    AuthProfileStore store(test_dir_.string());
    store.import_env_vars();
    // Don't assert anything specific since env vars may or may not be set
    // Just ensure it doesn't crash
}

TEST_F(AuthProfileStoreTest, AutoImportClaudeOAuth) {
    AuthProfileStore store(test_dir_.string());
    store.auto_import_claude_oauth();
    // Will only import if ~/.claude/.credentials.json exists
    // Just ensure no crash
}

TEST_F(AuthProfileStoreTest, AutoImportGeminiOAuth) {
    AuthProfileStore store(test_dir_.string());
    store.auto_import_gemini_oauth();
    // Will only import if ~/.gemini/oauth_creds.json exists
    // Just ensure no crash
}

// --- Coverage: compute_cooldown_ms billing escalation with multiple failures ---
TEST_F(AuthProfileStoreTest, BillingCooldownTripleFailure) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-1", "test");
    store.add_api_key("openai", "sk-2", "backup");

    auto auth = store.resolve("openai");
    ASSERT_TRUE(auth.has_value());

    // Three consecutive billing failures should escalate: 5h -> 10h -> 20h
    store.report_failure(auth->profile_id, CooldownReason::BillingError);
    store.report_failure(auth->profile_id, CooldownReason::BillingError);
    store.report_failure(auth->profile_id, CooldownReason::BillingError);

    auto remaining = store.cooldown_remaining_ms(auth->profile_id);
    // Should be at least 5 hours (base)
    EXPECT_GT(remaining, 5LL * 3600 * 1000 - 5000);
}

// --- Coverage: to_json with OAuth profiles containing all fields ---
TEST_F(AuthProfileStoreTest, ToJsonWithFullOAuthProfile) {
    AuthProfileStore store(test_dir_.string());
    store.add_oauth("anthropic", "access-tok", "refresh-tok",
                    9999999999999, "user@test.com");
    store.save();

    // Reload and verify all fields persisted
    AuthProfileStore store2(test_dir_.string());
    auto profiles = store2.profiles_for("anthropic");
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles[0].access_token, "access-tok");
    EXPECT_EQ(profiles[0].refresh_token, "refresh-tok");
    EXPECT_EQ(profiles[0].email, "user@test.com");
    EXPECT_EQ(profiles[0].expires_at, 9999999999999);
}

// --- Coverage: JSON round-trip with cooldowns including auth and billing reasons ---
TEST_F(AuthProfileStoreTest, JsonRoundTripWithAllCooldownTypes) {
    {
        AuthProfileStore store(test_dir_.string());
        store.add_api_key("openai", "sk-1", "test-a");
        store.add_api_key("openai", "sk-2", "test-b");
        store.add_api_key("anthropic", "sk-3", "test-c");

        auto auth_a = store.resolve("openai");
        store.report_failure(auth_a->profile_id, CooldownReason::AuthError);

        auto auth_c = store.resolve("anthropic");
        store.report_failure(auth_c->profile_id, CooldownReason::BillingError);

        store.save();
    }
    {
        AuthProfileStore store(test_dir_.string());
        // Should load without crashing
        auto all = store.all_profiles();
        EXPECT_GE(all.size(), 2u);
    }
}

// --- Coverage: resolve_with_fallback when primary is cooled down ---
TEST_F(AuthProfileStoreTest, ResolveWithFallbackCooledPrimary) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("anthropic", "sk-ant-1");
    store.add_api_key("openai", "sk-oai-1");

    // Cool down the anthropic profile
    auto auth = store.resolve("anthropic");
    ASSERT_TRUE(auth.has_value());
    store.report_failure(auth->profile_id, CooldownReason::RateLimit);

    // resolve_with_fallback should find openai through fallback chain
    auto fallback = store.resolve_with_fallback("anthropic");
    ASSERT_TRUE(fallback.has_value());
    EXPECT_EQ(fallback->provider, "openai");
}

// --- Coverage: rate limit escalation (1m -> 5m -> 25m) ---
TEST_F(AuthProfileStoreTest, RateLimitEscalation) {
    AuthProfileStore store(test_dir_.string());
    store.add_api_key("openai", "sk-1", "test");
    store.add_api_key("openai", "sk-2", "backup");

    auto auth = store.resolve("openai");
    ASSERT_TRUE(auth.has_value());

    store.report_failure(auth->profile_id, CooldownReason::RateLimit);
    auto remaining1 = store.cooldown_remaining_ms(auth->profile_id);

    store.report_failure(auth->profile_id, CooldownReason::RateLimit);
    auto remaining2 = store.cooldown_remaining_ms(auth->profile_id);

    store.report_failure(auth->profile_id, CooldownReason::RateLimit);
    auto remaining3 = store.cooldown_remaining_ms(auth->profile_id);

    EXPECT_GT(remaining2, remaining1);
    EXPECT_GT(remaining3, remaining2);
}

// NOLINTEND(bugprone-unchecked-optional-access)
