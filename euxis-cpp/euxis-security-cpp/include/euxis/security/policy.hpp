#pragma once

#include <expected>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::security {

class PolicyError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct SecurityPolicy {
    std::string auth_mode{"token"};
    bool require_https{false};
    bool require_mfa_for_elevated{true};
    bool allow_query_token{false};
    bool require_pii_filtering{true};
    bool require_audit_trail{true};
    bool require_human_oversight_for_critical{true};
    bool require_data_minimization{false};
    bool require_transparency_log{false};
    std::vector<std::string> allowed_scopes;

    [[nodiscard]] auto to_json() const -> nlohmann::json;
    [[nodiscard]] static auto from_json(const nlohmann::json& j) -> SecurityPolicy;
};

auto validate_auth_mode(const std::string& mode)
    -> std::expected<std::string, PolicyError>;

auto merge_policy(const nlohmann::json& overrides = nlohmann::json::object())
    -> std::expected<SecurityPolicy, PolicyError>;

} // namespace euxis::security
