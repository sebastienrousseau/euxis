#include "euxis/security/governance.hpp"

#include <algorithm>
#include <regex>

namespace euxis::security {

auto detect_pii(const std::string& text,
                 const std::vector<std::string>& patterns) -> std::vector<std::string> {
    std::vector<std::string> matches;
    for (const auto& pattern : patterns) {
        std::regex re(pattern, std::regex::ECMAScript | std::regex::icase);
        std::sregex_iterator it(text.begin(), text.end(), re);
        std::sregex_iterator end;
        for (; it != end; ++it) {
            matches.push_back(it->str());
        }
    }
    return matches;
}

auto audit_session(
    const std::vector<std::string>& messages,
    const GovernancePolicy& policy,
    const std::vector<std::string>& /*manifesto_allowed_tools*/,
    const std::string& authorization_scope) -> GovernanceResult {
    GovernanceResult result;

    // Check PII handling
    if (policy.require_pii_filtering && !policy.pii_patterns.empty()) {
        for (const auto& msg : messages) {
            auto pii_found = detect_pii(msg, policy.pii_patterns);
            if (!pii_found.empty()) {
                result.compliant = false;
                result.violations.push_back({
                    .check = GovernanceCheck::PiiHandling,
                    .description = "PII detected in message: " +
                        std::to_string(pii_found.size()) + " pattern(s) matched",
                    .severity = "critical",
                });
            }
        }
    }

    // Check authorization scope
    if (!authorization_scope.empty() && !policy.allowed_scopes.empty()) {
        auto it = std::ranges::find(policy.allowed_scopes, authorization_scope);
        if (it == policy.allowed_scopes.end()) {
            result.compliant = false;
            result.violations.push_back({
                .check = GovernanceCheck::AuthorizationScope,
                .description = "Scope '" + authorization_scope + "' not in allowed list",
                .severity = "high",
            });
        }
    }

    // Check audit trail (messages must exist)
    if (policy.require_audit_trail && messages.empty()) {
        result.compliant = false;
        result.violations.push_back({
            .check = GovernanceCheck::AuditTrail,
            .description = "No audit trail: session has zero messages",
            .severity = "medium",
        });
    }

    // Data minimization: flag very long messages
    if (policy.require_data_minimization) {
        for (const auto& msg : messages) {
            if (msg.size() > 100000) {
                result.compliant = false;
                result.violations.push_back({
                    .check = GovernanceCheck::DataMinimization,
                    .description = "Message exceeds 100KB — data minimization violation",
                    .severity = "medium",
                });
                break;
            }
        }
    }

    return result;
}

auto check_action_authorization(
    const std::string& /*agent_did*/,
    const std::string& action,
    const std::string& agent_scope,
    const GovernancePolicy& policy) -> std::expected<void, GovernanceViolation> {
    // If policy has allowed scopes, agent_scope must be in the list
    if (!policy.allowed_scopes.empty()) {
        auto it = std::ranges::find(policy.allowed_scopes, agent_scope);
        if (it == policy.allowed_scopes.end()) {
            return std::unexpected(GovernanceViolation{
                .check = GovernanceCheck::AuthorizationScope,
                .description = "Agent scope '" + agent_scope +
                    "' not authorized for action '" + action + "'",
                .severity = "critical",
            });
        }
    }

    // Critical actions require human oversight
    if (policy.require_human_oversight_for_critical) {
        static const std::vector<std::string> critical_actions = {
            "delete", "destroy", "deploy", "publish", "transfer",
        };
        auto it = std::ranges::find(critical_actions, action);
        if (it != critical_actions.end()) {
            return std::unexpected(GovernanceViolation{
                .check = GovernanceCheck::HumanOversight,
                .description = "Action '" + action + "' requires human oversight",
                .severity = "high",
            });
        }
    }

    return {};
}

} // namespace euxis::security
