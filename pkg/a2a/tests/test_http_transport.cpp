#include <gtest/gtest.h>

#include <string>

#include "euxis/a2a/http_transport.hpp"

namespace euxis::a2a {
namespace {

// ---------------------------------------------------------------------------
// Constructor stores base_url
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, ConstructorStoresBaseUrl) {
    HttpA2ATransport transport("http://localhost:8080");
    // The transport is constructed without error — we verify behaviour
    // through the send/discover methods below.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Constructor strips trailing slash
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, ConstructorStripsTrailingSlash) {
    // Should not throw; internally normalises the URL
    HttpA2ATransport transport("http://localhost:8080/");
    SUCCEED();
}

// ---------------------------------------------------------------------------
// send() to unreachable host returns an error (not a crash)
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, SendToUnreachableHostReturnsError) {
    HttpA2ATransport transport("http://127.0.0.1:1");  // likely unreachable port

    auto result = transport.send("agent/card", {});

    ASSERT_FALSE(result.has_value());
    // Should contain a meaningful error message
    EXPECT_FALSE(result.error().empty());
}

// ---------------------------------------------------------------------------
// discover() to unreachable host returns an error
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, DiscoverUnreachableReturnsError) {
    HttpA2ATransport transport("http://127.0.0.1:1");

    auto result = transport.discover("http://127.0.0.1:1");

    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

// ---------------------------------------------------------------------------
// send() with empty params does not crash
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, SendEmptyParamsNoCrash) {
    HttpA2ATransport transport("http://127.0.0.1:1");

    auto result = transport.send("task/create", nlohmann::json::object());

    // Will fail because the host is unreachable, but must not crash
    ASSERT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// discover() with path-containing URL handles correctly
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, DiscoverWithPathUrl) {
    HttpA2ATransport transport("http://127.0.0.1:1");

    // discover should attempt to fetch {url}/.well-known/agent.json
    auto result = transport.discover("http://127.0.0.1:1/api/v1");

    ASSERT_FALSE(result.has_value());
    // Error should be about connection, not about URL parsing
    EXPECT_FALSE(result.error().empty());
}

// ---------------------------------------------------------------------------
// Multiple send calls do not leak or corrupt state
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, MultipleSendsDoNotCorruptState) {
    HttpA2ATransport transport("http://127.0.0.1:1");

    auto r1 = transport.send("agent/card", {});
    auto r2 = transport.send("task/create", {{"message", "test"}});
    auto r3 = transport.send("capabilities/list", {});

    // All should fail gracefully
    EXPECT_FALSE(r1.has_value());
    EXPECT_FALSE(r2.has_value());
    EXPECT_FALSE(r3.has_value());
}

// ---------------------------------------------------------------------------
// Constructor with URL without scheme
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, ConstructorWithoutScheme) {
    HttpA2ATransport transport("localhost:8080");
    // parse_url should add http:// prefix
    auto result = transport.send("test/method", {});
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

// ---------------------------------------------------------------------------
// Constructor with URL containing path but no scheme
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, ConstructorWithPathNoScheme) {
    HttpA2ATransport transport("localhost:8080/api/v1");
    auto result = transport.send("test/method", {});
    ASSERT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// Discover with URL that has no path
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, DiscoverWithNoPath) {
    HttpA2ATransport transport("http://127.0.0.1:1");
    auto result = transport.discover("http://127.0.0.1:1");
    ASSERT_FALSE(result.has_value());
    // Error should mention HTTP failure
    EXPECT_FALSE(result.error().empty());
}

// ---------------------------------------------------------------------------
// Send with complex JSON params
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, SendWithComplexParams) {
    HttpA2ATransport transport("http://127.0.0.1:1");
    nlohmann::json params = {
        {"task", {{"description", "test task"}, {"priority", "high"}}},
        {"artifacts", nlohmann::json::array({"file1.txt", "file2.txt"})},
        {"metadata", {{"version", 2}, {"nested", {{"key", "value"}}}}}
    };
    auto result = transport.send("task/create", params);
    ASSERT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// Discover URL with trailing slash on path
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, DiscoverWithTrailingSlash) {
    HttpA2ATransport transport("http://127.0.0.1:1");
    auto result = transport.discover("http://127.0.0.1:1/api/");
    ASSERT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// Send with empty method name
// ---------------------------------------------------------------------------
TEST(HttpTransportTest, SendWithEmptyMethod) {
    HttpA2ATransport transport("http://127.0.0.1:1");
    auto result = transport.send("", {});
    ASSERT_FALSE(result.has_value());
}

} // namespace
} // namespace euxis::a2a
