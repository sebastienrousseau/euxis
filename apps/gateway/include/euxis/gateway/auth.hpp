#pragma once

#include <expected>
#include <string>

namespace euxis::gateway {

struct AuthError {
    int status_code;
    std::string message;
};

/// Verify HMAC-SHA256 signature.
[[nodiscard]] auto verify_hmac(const std::string& payload,
                               const std::string& signature,
                               const std::string& secret)
    -> std::expected<bool, AuthError>;

/// Verify bearer token.
[[nodiscard]] auto verify_bearer_token(const std::string& token,
                                       const std::string& expected)
    -> std::expected<bool, AuthError>;

} // namespace euxis::gateway
