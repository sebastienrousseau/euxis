#include <gtest/gtest.h>
#include "euxis/cli/pii_filter.hpp"

#include <cstdlib>

namespace euxis::cli {
namespace {

TEST(PiiFilterTest, RedactEmail) {
    auto result = PiiFilter::redact("Contact user@example.com for info");
    EXPECT_NE(result.find("[EMAIL]"), std::string::npos);
    EXPECT_EQ(result.find("user@example.com"), std::string::npos);
}

TEST(PiiFilterTest, RedactApiKey) {
    auto result = PiiFilter::redact("Using sk_live_abcdefghijklmnopqrstuvwxyz");
    EXPECT_NE(result.find("[API_KEY]"), std::string::npos);
}

TEST(PiiFilterTest, RedactBearerToken) {
    auto result = PiiFilter::redact("Authorization: Bearer abc123.xyz789");
    EXPECT_NE(result.find("[TOKEN]"), std::string::npos);
    EXPECT_EQ(result.find("abc123"), std::string::npos);
}

TEST(PiiFilterTest, RedactIPv4) {
    auto result = PiiFilter::redact("Server at 192.168.1.100");
    EXPECT_NE(result.find("[IP]"), std::string::npos);
    EXPECT_EQ(result.find("192.168.1.100"), std::string::npos);
}

TEST(PiiFilterTest, PreserveLocalhost) {
    auto result = PiiFilter::redact("Listening on 127.0.0.1:8080");
    EXPECT_NE(result.find("127.0.0.1"), std::string::npos);
}

TEST(PiiFilterTest, NoRedactionWhenDisabled) {
    setenv("EUXIS_LOG_SANITIZE", "0", 1);
    auto result = PiiFilter::redact("user@example.com");
    EXPECT_NE(result.find("user@example.com"), std::string::npos);
    unsetenv("EUXIS_LOG_SANITIZE");
}

TEST(PiiFilterTest, CleanTextUnchanged) {
    auto input = "This is clean text with no PII";
    EXPECT_EQ(PiiFilter::redact(input), input);
}

} // namespace
} // namespace euxis::cli
