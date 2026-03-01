#include "euxis/security/policy.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace euxis::security {
namespace {

constexpr std::array kValidAuthModes{"token", "password", "none"};

auto to_lower(std::string s) -> std::string {
    std::ranges::transform(s, s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
    return s;
}

auto trim(std::string s) -> std::string {
    auto start = std::ranges::find_if_not(s, ::isspace);
    auto end = std::find_if_not(s.rbegin(), s.rend(),
                                [](unsigned char c) { return std::isspace(c); })
                   .base();
    if (start >= end) return {};
    return {start, end};
}

} // namespace

auto SecurityPolicy::to_json() const -> nlohmann::json {
    return {
        {"auth_mode", auth_mode},
        {"require_https", require_https},
        {"require_mfa_for_elevated", require_mfa_for_elevated},
        {"allow_query_token", allow_query_token},
    };
}

auto SecurityPolicy::from_json(const nlohmann::json& j) -> SecurityPolicy {
    SecurityPolicy p;
    if (j.contains("auth_mode")) p.auth_mode = j["auth_mode"].get<std::string>();
    if (j.contains("require_https")) p.require_https = j["require_https"].get<bool>();
    if (j.contains("require_mfa_for_elevated"))
        p.require_mfa_for_elevated = j["require_mfa_for_elevated"].get<bool>();
    if (j.contains("allow_query_token"))
        p.allow_query_token = j["allow_query_token"].get<bool>();
    return p;
}

auto validate_auth_mode(const std::string& mode)
    -> std::expected<std::string, PolicyError> {
    auto normalized = to_lower(trim(mode));
    for (const auto* valid : kValidAuthModes) {
        if (normalized == valid) return normalized;
    }
    return std::unexpected(PolicyError("Unsupported auth mode: " + mode));
}

auto merge_policy(const nlohmann::json& overrides)
    -> std::expected<SecurityPolicy, PolicyError> {
    SecurityPolicy base;
    if (overrides.empty() || overrides.is_null()) return base;

    std::string raw_mode =
        overrides.value("auth_mode", base.auth_mode);
    auto mode = validate_auth_mode(raw_mode);
    if (!mode) return std::unexpected(mode.error());

    return SecurityPolicy{
        .auth_mode = *mode,
        .require_https = overrides.value("require_https", base.require_https),
        .require_mfa_for_elevated =
            overrides.value("require_mfa_for_elevated", base.require_mfa_for_elevated),
        .allow_query_token = overrides.value("allow_query_token", base.allow_query_token),
    };
}

} // namespace euxis::security
