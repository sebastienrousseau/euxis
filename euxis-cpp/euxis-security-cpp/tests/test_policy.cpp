#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "euxis/security/policy.hpp"

namespace euxis::security {
namespace {

TEST(SecurityPolicyTest, DefaultValues) {
    SecurityPolicy p;
    EXPECT_EQ(p.auth_mode, "token");
    EXPECT_FALSE(p.require_https);
    EXPECT_TRUE(p.require_mfa_for_elevated);
    EXPECT_FALSE(p.allow_query_token);
}

TEST(SecurityPolicyTest, ToJsonRoundtrip) {
    SecurityPolicy p;
    p.auth_mode = "password";
    p.require_https = true;
    auto j = p.to_json();
    auto restored = SecurityPolicy::from_json(j);
    EXPECT_EQ(restored.auth_mode, "password");
    EXPECT_TRUE(restored.require_https);
}

TEST(ValidateAuthModeTest, AcceptsValidModes) {
    EXPECT_EQ(validate_auth_mode("token").value(), "token");
    EXPECT_EQ(validate_auth_mode("PASSWORD").value(), "password");
    EXPECT_EQ(validate_auth_mode("  None  ").value(), "none");
}

TEST(ValidateAuthModeTest, RejectsInvalidMode) {
    auto result = validate_auth_mode("oauth2");
    EXPECT_FALSE(result.has_value());
}

TEST(MergePolicyTest, EmptyOverridesReturnDefaults) {
    auto result = merge_policy();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->auth_mode, "token");
}

TEST(MergePolicyTest, OverridesApplied) {
    nlohmann::json overrides = {
        {"auth_mode", "password"},
        {"require_https", true},
        {"allow_query_token", true},
    };
    auto result = merge_policy(overrides);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->auth_mode, "password");
    EXPECT_TRUE(result->require_https);
    EXPECT_TRUE(result->allow_query_token);
}

TEST(MergePolicyTest, InvalidModeReturnsError) {
    nlohmann::json overrides = {{"auth_mode", "bogus"}};
    auto result = merge_policy(overrides);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateAuthModeTest, RejectsInvalidModeContainsMessage) {
    auto result = validate_auth_mode("jwt");
    ASSERT_FALSE(result.has_value());
    // Verify the error message includes the invalid mode
    try {
        std::rethrow_exception(std::make_exception_ptr(result.error()));
    } catch (const PolicyError& e) {
        EXPECT_TRUE(std::string(e.what()).find("jwt") != std::string::npos);
    }
}

TEST(SecurityPolicyTest, FromJsonPartialFields) {
    nlohmann::json j = {
        {"auth_mode", "none"},
        {"require_pii_filtering", false},
        {"require_transparency_log", false},
        {"allowed_scopes", std::vector<std::string>{"read", "write"}},
    };
    auto p = SecurityPolicy::from_json(j);
    EXPECT_EQ(p.auth_mode, "none");
    EXPECT_FALSE(p.require_pii_filtering);
    EXPECT_FALSE(p.require_transparency_log);
    ASSERT_EQ(p.allowed_scopes.size(), 2);
    EXPECT_EQ(p.allowed_scopes[0], "read");
}

} // namespace
} // namespace euxis::security
