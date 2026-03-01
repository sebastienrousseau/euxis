#include <gtest/gtest.h>
#include <sodium.h>

#include "euxis/gateway/auth.hpp"

namespace euxis::gateway {
namespace {

class AuthTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0);
    }
};

TEST_F(AuthTest, ValidHmac) {
    std::string secret = "test-secret";
    std::string payload = "hello world";

    // Compute expected signature
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

    std::string hex;
    char buf[3];
    for (auto b : mac) {
        std::snprintf(buf, sizeof(buf), "%02x", b);
        hex += buf;
    }

    auto result = verify_hmac(payload, hex, secret);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(AuthTest, InvalidHmacRejects) {
    auto result = verify_hmac("payload", "bad-sig", "secret");
    EXPECT_FALSE(result.has_value());
}

TEST_F(AuthTest, EmptySecretRejects) {
    auto result = verify_hmac("payload", "sig", "");
    EXPECT_FALSE(result.has_value());
}

TEST_F(AuthTest, ValidBearerToken) {
    auto result = verify_bearer_token("my-token", "my-token");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(AuthTest, InvalidBearerToken) {
    auto result = verify_bearer_token("wrong", "expected");
    EXPECT_FALSE(result.has_value());
}

TEST_F(AuthTest, NoAuthConfigured) {
    auto result = verify_bearer_token("anything", "");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

} // namespace
} // namespace euxis::gateway
