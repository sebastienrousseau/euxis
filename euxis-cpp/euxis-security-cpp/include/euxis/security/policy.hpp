#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "errors.hpp"

namespace euxis::security {

struct SecurityPolicy {
    std::string auth_mode{"token"};
    bool require_https{false};
    bool require_mfa_for_elevated{true};
    bool allow_query_token{false};

    // NIST AI RMF governance fields
    bool require_pii_filtering{true};
    bool require_audit_trail{true};
    bool require_human_oversight_for_critical{true};
    bool require_data_minimization{true};
    bool require_transparency_log{true};
    std::vector<std::string> allowed_scopes;

    [[nodiscard]] auto to_json() const -> nlohmann::json;
    static auto from_json(const nlohmann::json& j) -> SecurityPolicy;
};

/// Normalize and validate auth mode.  Returns the lowercase mode on success.
[[nodiscard]] auto validate_auth_mode(const std::string& mode)
    -> std::expected<std::string, PolicyError>;

/// Create a policy from defaults plus optional overrides.
[[nodiscard]] auto merge_policy(const nlohmann::json& overrides = nlohmann::json{})
    -> std::expected<SecurityPolicy, PolicyError>;

} // namespace euxis::security
