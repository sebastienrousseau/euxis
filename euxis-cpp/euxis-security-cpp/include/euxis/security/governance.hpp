#pragma once

#include <expected>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::security {

enum class GovernanceCheck {
    PiiHandling,
    AuthorizationScope,
    AuditTrail,
    HumanOversight,
    OutputVerification,
    DataMinimization,
    TransparencyLog,
};

struct GovernanceViolation {
    GovernanceCheck check;
    std::string description;
    std::string severity; // "critical", "high", "medium", "low"
};

struct GovernanceResult {
    bool compliant{true};
    std::vector<GovernanceViolation> violations;
};

struct GovernancePolicy {
    bool require_pii_filtering{true};
    bool require_audit_trail{true};
    bool require_human_oversight_for_critical{true};
    bool require_output_verification{false};
    bool require_data_minimization{true};
    bool require_transparency_log{true};
    std::vector<std::string> allowed_scopes;
    std::vector<std::string> pii_patterns; // regex patterns for PII detection
};

/// Audit a conversation session for governance compliance.
[[nodiscard]] auto audit_session(
    const std::vector<std::string>& messages,
    const GovernancePolicy& policy,
    const std::vector<std::string>& manifesto_allowed_tools = {},
    const std::string& authorization_scope = "") -> GovernanceResult;

/// Check whether an action is authorized given agent scope and policy.
[[nodiscard]] auto check_action_authorization(
    const std::string& agent_did,
    const std::string& action,
    const std::string& agent_scope,
    const GovernancePolicy& policy) -> std::expected<void, GovernanceViolation>;

/// Detect PII patterns in text.
[[nodiscard]] auto detect_pii(const std::string& text,
                               const std::vector<std::string>& patterns)
    -> std::vector<std::string>;

} // namespace euxis::security
