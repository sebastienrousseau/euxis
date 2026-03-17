#include "euxis/gateway/auth.hpp"

#include <sodium.h>

#include <cstring>
#include <string>

namespace euxis::gateway {

auto verify_hmac(const std::string& payload,
                 const std::string& signature,
                 const std::string& secret)
    -> std::expected<bool, AuthError> {
    if (secret.empty())
        return std::unexpected(AuthError{401, "Missing HMAC secret"});

    unsigned char mac[crypto_auth_hmacsha256_BYTES];
    crypto_auth_hmacsha256_state state;
    crypto_auth_hmacsha256_init(
        &state,
        reinterpret_cast<const unsigned char*>(secret.data()),
        secret.size());
    crypto_auth_hmacsha256_update(
        &state,
        reinterpret_cast<const unsigned char*>(payload.data()),
        payload.size());
    crypto_auth_hmacsha256_final(&state, mac);

    // Convert to hex using libsodium (safe, no format specifier issues)
    char hex_buf[crypto_auth_hmacsha256_BYTES * 2 + 1];
    sodium_bin2hex(hex_buf, sizeof(hex_buf), mac, crypto_auth_hmacsha256_BYTES);
    std::string computed(hex_buf);

    // Constant-time compare
    if (signature.size() != computed.size())
        return std::unexpected(AuthError{401, "Invalid signature"});

    if (sodium_memcmp(signature.data(), computed.data(), computed.size()) != 0)
        return std::unexpected(AuthError{401, "Invalid signature"});

    return true;
}

auto verify_bearer_token(const std::string& token,
                         const std::string& expected)
    -> std::expected<bool, AuthError> {
    if (expected.empty())
        return std::unexpected(AuthError{500, "No auth token configured"});
    if (token.empty())
        return std::unexpected(AuthError{401, "Missing bearer token"});

    // Constant-time compare
    if (token.size() != expected.size() ||
        sodium_memcmp(token.data(), expected.data(), expected.size()) != 0)
        return std::unexpected(AuthError{401, "Invalid bearer token"});

    return true;
}

} // namespace euxis::gateway
