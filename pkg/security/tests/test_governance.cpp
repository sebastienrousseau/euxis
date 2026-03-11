#include <gtest/gtest.h>

#include "euxis/security/governance.hpp"

namespace euxis::security {
namespace {

TEST(GovernanceTest, CleanSessionPasses) {
    GovernancePolicy policy;
    std::vector<std::string> messages = {"Hello", "How can I help?"};
    auto result = audit_session(messages, policy);
    EXPECT_TRUE(result.compliant);
    EXPECT_TRUE(result.violations.empty());
}

TEST(GovernanceTest, PiiDetection) {
    GovernancePolicy policy;
    policy.pii_patterns = {
        R"(\b\d{3}-\d{2}-\d{4}\b)",       // SSN
        R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b)", // email
    };

    auto pii = detect_pii("My SSN is 123-45-6789 and email is test@example.com",
                            policy.pii_patterns);
    EXPECT_GE(pii.size(), 2u);
}

TEST(GovernanceTest, PiiViolationInSession) {
    GovernancePolicy policy;
    policy.pii_patterns = {R"(\b\d{3}-\d{2}-\d{4}\b)"};

    std::vector<std::string> messages = {
        "Please process SSN 123-45-6789",
    };

    auto result = audit_session(messages, policy);
    EXPECT_FALSE(result.compliant);
    ASSERT_FALSE(result.violations.empty());
    EXPECT_EQ(result.violations[0].check, GovernanceCheck::PiiHandling);
    EXPECT_EQ(result.violations[0].severity, "critical");
}

TEST(GovernanceTest, ScopeViolation) {
    GovernancePolicy policy;
    policy.allowed_scopes = {"read", "write"};

    auto result = audit_session({"msg"}, policy, {}, "admin");
    EXPECT_FALSE(result.compliant);
    ASSERT_FALSE(result.violations.empty());
    EXPECT_EQ(result.violations[0].check, GovernanceCheck::AuthorizationScope);
}

TEST(GovernanceTest, ScopeValid) {
    GovernancePolicy policy;
    policy.allowed_scopes = {"read", "write"};

    auto result = audit_session({"msg"}, policy, {}, "read");
    EXPECT_TRUE(result.compliant);
}

TEST(GovernanceTest, AuditTrailViolation) {
    GovernancePolicy policy;
    policy.require_audit_trail = true;

    auto result = audit_session({}, policy);
    EXPECT_FALSE(result.compliant);
    ASSERT_FALSE(result.violations.empty());
    EXPECT_EQ(result.violations[0].check, GovernanceCheck::AuditTrail);
}

TEST(GovernanceTest, ActionAuthorizationValid) {
    GovernancePolicy policy;
    policy.allowed_scopes = {"read", "write"};

    auto result = check_action_authorization("did:euxis:agent-01", "read_file",
                                              "read", policy);
    EXPECT_TRUE(result.has_value());
}

TEST(GovernanceTest, ActionAuthorizationScopeViolation) {
    GovernancePolicy policy;
    policy.allowed_scopes = {"read"};

    auto result = check_action_authorization("did:euxis:agent-01", "write_file",
                                              "admin", policy);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().check, GovernanceCheck::AuthorizationScope);
}

TEST(GovernanceTest, CriticalActionRequiresHumanOversight) {
    GovernancePolicy policy;
    policy.require_human_oversight_for_critical = true;

    auto result = check_action_authorization("did:euxis:agent-01", "delete",
                                              "admin", policy);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().check, GovernanceCheck::HumanOversight);
}

TEST(GovernanceTest, DataMinimizationViolation) {
    GovernancePolicy policy;
    policy.require_data_minimization = true;

    std::string huge_message(200000, 'x');
    auto result = audit_session({huge_message}, policy);
    EXPECT_FALSE(result.compliant);

    bool found_dm = false;
    for (const auto& v : result.violations) {
        if (v.check == GovernanceCheck::DataMinimization) {
            found_dm = true;
        }
    }
    EXPECT_TRUE(found_dm);
}

// --- Coverage: line 20 (detect_pii with no matches) ---
TEST(GovernanceTest, NoPiiDetectedInCleanText) {
    auto pii = detect_pii("This is a clean message with no PII",
                          {R"(\b\d{3}-\d{2}-\d{4}\b)"});
    EXPECT_TRUE(pii.empty());
}

// --- Coverage: line 39 (PII filtering disabled when patterns empty) ---
TEST(GovernanceTest, PiiFilteringDisabledWhenPatternsEmpty) {
    GovernancePolicy policy;
    policy.pii_patterns = {};  // no patterns
    std::vector<std::string> messages = {"SSN: 123-45-6789"};
    auto result = audit_session(messages, policy);
    EXPECT_TRUE(result.compliant);
}

// --- Coverage: line 53 (scope check skipped when scope is empty) ---
TEST(GovernanceTest, EmptyScopeSkipsCheck) {
    GovernancePolicy policy;
    policy.allowed_scopes = {"read", "write"};
    // Pass empty authorization_scope
    auto result = audit_session({"msg"}, policy, {}, "");
    EXPECT_TRUE(result.compliant);
}

// --- Coverage: line 64 (audit trail with non-empty messages passes) ---
TEST(GovernanceTest, AuditTrailWithMessagesPasses) {
    GovernancePolicy policy;
    policy.require_audit_trail = true;
    auto result = audit_session({"msg1", "msg2"}, policy);
    EXPECT_TRUE(result.compliant);
}

// --- Coverage: lines 75-76 (data minimization with small messages passes) ---
TEST(GovernanceTest, DataMinimizationSmallMessagesPasses) {
    GovernancePolicy policy;
    policy.require_data_minimization = true;
    auto result = audit_session({"short msg"}, policy);
    EXPECT_TRUE(result.compliant);
}

// --- Coverage: line 75 (data minimization disabled when opted out) ---
TEST(GovernanceTest, DataMinimizationDisabledSkipsCheck) {
    GovernancePolicy policy;
    policy.require_data_minimization = false;
    policy.require_pii_filtering = false;
    policy.require_audit_trail = false;
    std::string huge(200000, 'x');
    auto result = audit_session({huge}, policy);
    EXPECT_TRUE(result.compliant);
}

} // namespace
} // namespace euxis::security
