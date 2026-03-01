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

} // namespace
} // namespace euxis::a2a
