/// @file
/// @brief Governance and compliance checking for agent operations.
#pragma once

#include <expected>
#include <string>
#include <vector>

namespace euxis::security {

/// @brief types of governance checks that can be performed.
enum class GovernanceCheck {
    PiiHandling,
    AuthorizationScope,
    AuditTrail,
    HumanOversight,
    DataMinimization,
    TransparencyLog
};

/// @brief record of a specific governance violation.
struct GovernanceViolation {
    GovernanceCheck check;
    std::string description;
    std::string severity; // "critical", "high", "medium", "low"
};

/// @brief Result of a compliance audit.
struct GovernanceResult {
    bool compliant{true};
    std::vector<GovernanceViolation> violations;
};

/// @brief Unified policy for governing agent behavior.
struct GovernancePolicy {
    bool require_pii_filtering{true};
    std::vector<std::string> pii_patterns;
    std::vector<std::string> allowed_scopes;
    bool require_audit_trail{true};
    bool require_human_oversight_for_critical{true};
    bool require_data_minimization{false};
};

/// @brief Scan text for Personally Identifiable Information (PII).
auto detect_pii(const std::string& text,
                 const std::vector<std::string>& patterns) -> std::vector<std::string>;

/// @brief audit a conversation session against a governance policy.
auto audit_session(
    const std::vector<std::string>& messages,
    const GovernancePolicy& policy,
    const std::vector<std::string>& manifesto_allowed_tools = {},
    const std::string& authorization_scope = "") -> GovernanceResult;

/// @brief verify if a specific action is authorized under the current policy.
auto check_action_authorization(
    const std::string& agent_did,
    const std::string& action,
    const std::string& agent_scope,
    const GovernancePolicy& policy) -> std::expected<void, GovernanceViolation>;

} // namespace euxis::security
