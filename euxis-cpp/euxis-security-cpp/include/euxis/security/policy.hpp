#pragma once

#include <expected>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "errors.hpp"

namespace euxis::security {

struct SecurityPolicy {
    std::string auth_mode{"token"};
    bool require_https{false};
    bool require_mfa_for_elevated{true};
    bool allow_query_token{false};

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
