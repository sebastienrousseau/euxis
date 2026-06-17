/// @file
/// @brief Tests for Anthropic prompt-cache helpers.

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "euxis/inference/cache_control.hpp"

namespace euxis::inference {
namespace {

using nlohmann::json;

// ---------------------------------------------------------------------------
// Empty / malformed inputs return 0 and do not throw
// ---------------------------------------------------------------------------
TEST(CacheControlTest, EmptyArrayMarksNothing) {
    json messages = json::array();
    EXPECT_EQ(apply_anthropic_cache_control(messages), 0u);
    EXPECT_TRUE(messages.is_array());
    EXPECT_TRUE(messages.empty());
}

TEST(CacheControlTest, NonArrayInputMarksNothing) {
    json messages = json::object();
    EXPECT_EQ(apply_anthropic_cache_control(messages), 0u);
}

// ---------------------------------------------------------------------------
// Single system message gets one marker
// ---------------------------------------------------------------------------
TEST(CacheControlTest, SystemOnlyGetsOneMarker) {
    json messages = json::array({
        {{"role", "system"}, {"content", "You are an audit agent."}},
    });
    EXPECT_EQ(apply_anthropic_cache_control(messages), 1u);
    ASSERT_TRUE(messages[0]["content"].is_array());
    EXPECT_EQ(messages[0]["content"].back()["cache_control"]["type"], "ephemeral");
    EXPECT_EQ(messages[0]["content"].back()["cache_control"]["ttl"],  "5m");
}

// ---------------------------------------------------------------------------
// System + user gets two markers, default TTL
// ---------------------------------------------------------------------------
TEST(CacheControlTest, SystemAndUserGetTwoMarkers) {
    json messages = json::array({
        {{"role", "system"}, {"content", "S"}},
        {{"role", "user"},   {"content", "U"}},
    });
    EXPECT_EQ(apply_anthropic_cache_control(messages), 2u);
    EXPECT_EQ(messages[0]["content"].back()["cache_control"]["ttl"], "5m");
    EXPECT_EQ(messages[1]["content"].back()["cache_control"]["ttl"], "5m");
}

// ---------------------------------------------------------------------------
// Custom TTL is honoured
// ---------------------------------------------------------------------------
TEST(CacheControlTest, CustomTtlIsHonoured) {
    json messages = json::array({
        {{"role", "system"}, {"content", "S"}},
        {{"role", "user"},   {"content", "U"}},
    });
    EXPECT_EQ(apply_anthropic_cache_control(messages, "1h"), 2u);
    EXPECT_EQ(messages[0]["content"].back()["cache_control"]["ttl"], "1h");
    EXPECT_EQ(messages[1]["content"].back()["cache_control"]["ttl"], "1h");
}

// ---------------------------------------------------------------------------
// Multi-turn conversation marks only the LAST system and LAST user message
// ---------------------------------------------------------------------------
TEST(CacheControlTest, OnlyLastSystemAndLastUserAreMarked) {
    json messages = json::array({
        {{"role", "system"},    {"content", "S1"}},
        {{"role", "user"},      {"content", "U1"}},
        {{"role", "assistant"}, {"content", "A1"}},
        {{"role", "user"},      {"content", "U2"}},
        {{"role", "system"},    {"content", "S2"}}, // late re-init of system
        {{"role", "user"},      {"content", "U3"}},
    });
    EXPECT_EQ(apply_anthropic_cache_control(messages), 2u);

    // S1 has no marker (S2 is the most recent system).
    EXPECT_FALSE(messages[0]["content"].back().contains("cache_control"));
    // U1, A1, U2 have no marker.
    EXPECT_FALSE(messages[1]["content"].back().contains("cache_control"));
    EXPECT_FALSE(messages[2]["content"].back().contains("cache_control"));
    EXPECT_FALSE(messages[3]["content"].back().contains("cache_control"));
    // S2 (most recent system) is marked.
    EXPECT_TRUE(messages[4]["content"].back().contains("cache_control"));
    // U3 (most recent user) is marked.
    EXPECT_TRUE(messages[5]["content"].back().contains("cache_control"));
}

// ---------------------------------------------------------------------------
// String content gets normalised to a content-block array first
// ---------------------------------------------------------------------------
TEST(CacheControlTest, StringContentIsNormalised) {
    json messages = json::array({
        {{"role", "system"}, {"content", "raw string body"}},
    });
    EXPECT_EQ(apply_anthropic_cache_control(messages), 1u);
    ASSERT_TRUE(messages[0]["content"].is_array());
    ASSERT_EQ(messages[0]["content"].size(), 1u);
    EXPECT_EQ(messages[0]["content"][0]["type"], "text");
    EXPECT_EQ(messages[0]["content"][0]["text"], "raw string body");
}

// ---------------------------------------------------------------------------
// Multi-block array marks only the LAST block of the targeted message
// ---------------------------------------------------------------------------
TEST(CacheControlTest, OnlyLastBlockOfTargetedMessageIsMarked) {
    json messages = json::array({
        {{"role", "user"}, {"content", json::array({
            {{"type", "text"}, {"text", "first part"}},
            {{"type", "text"}, {"text", "second part"}},
            {{"type", "text"}, {"text", "tail"}},
        })}},
    });
    EXPECT_EQ(apply_anthropic_cache_control(messages), 1u);

    EXPECT_FALSE(messages[0]["content"][0].contains("cache_control"));
    EXPECT_FALSE(messages[0]["content"][1].contains("cache_control"));
    EXPECT_TRUE(messages[0]["content"][2].contains("cache_control"));
}

// ---------------------------------------------------------------------------
// Idempotent: re-applying does not duplicate the marker
// ---------------------------------------------------------------------------
TEST(CacheControlTest, ReapplicationIsIdempotent) {
    json messages = json::array({
        {{"role", "system"}, {"content", "S"}},
        {{"role", "user"},   {"content", "U"}},
    });
    EXPECT_EQ(apply_anthropic_cache_control(messages), 2u);
    EXPECT_EQ(apply_anthropic_cache_control(messages), 0u) << "no new markers";

    // Marker payload is unchanged.
    EXPECT_EQ(messages[0]["content"].back()["cache_control"]["type"], "ephemeral");
    EXPECT_EQ(messages[1]["content"].back()["cache_control"]["type"], "ephemeral");
}

// ---------------------------------------------------------------------------
// Functional variant returns a marked copy, leaves input untouched
// ---------------------------------------------------------------------------
TEST(CacheControlTest, FunctionalVariantDoesNotMutateInput) {
    json input = json::array({
        {{"role", "system"}, {"content", "S"}},
        {{"role", "user"},   {"content", "U"}},
    });
    auto out = with_anthropic_cache_control(input);

    // Input untouched.
    EXPECT_TRUE(input[0]["content"].is_string());
    EXPECT_TRUE(input[1]["content"].is_string());

    // Output has markers.
    EXPECT_TRUE(out[0]["content"].back().contains("cache_control"));
    EXPECT_TRUE(out[1]["content"].back().contains("cache_control"));
}

// ---------------------------------------------------------------------------
// Messages without any system/user role get nothing
// ---------------------------------------------------------------------------
TEST(CacheControlTest, AssistantOnlyMarksNothing) {
    json messages = json::array({
        {{"role", "assistant"}, {"content", "A"}},
    });
    EXPECT_EQ(apply_anthropic_cache_control(messages), 0u);
    EXPECT_FALSE(messages[0]["content"].back().contains("cache_control"));
}

} // namespace
} // namespace euxis::inference
