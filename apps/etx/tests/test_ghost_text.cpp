#include <gtest/gtest.h>
#include <euxis/etx/ghost_text.hpp>

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::etx {
namespace {

TEST(GhostTextTest, SuggestEmptyPrefix) {
    GhostTextEngine engine;
    engine.record("hello world");
    EXPECT_FALSE(engine.suggest("").has_value());
}

TEST(GhostTextTest, SuggestNoMatch) {
    GhostTextEngine engine;
    engine.record("hello world");
    EXPECT_FALSE(engine.suggest("xyz").has_value());
}

TEST(GhostTextTest, SuggestBasicMatch) {
    GhostTextEngine engine;
    engine.record("hello world");
    auto result = engine.suggest("hel");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "hello world");
}

TEST(GhostTextTest, SuggestByFrequency) {
    GhostTextEngine engine;
    engine.record("deploy agent");
    engine.record("deploy squad");
    engine.record("deploy squad");
    engine.record("deploy squad");

    auto result = engine.suggest("deploy");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "deploy squad");  // higher frequency
}

TEST(GhostTextTest, SuggestExactMatchSkipped) {
    GhostTextEngine engine;
    engine.record("test");
    // Exact match returns nullopt (no completion needed)
    EXPECT_FALSE(engine.suggest("test").has_value());
}

TEST(GhostTextTest, LoadFrequencies) {
    GhostTextEngine engine;
    std::flat_map<std::string, int> data;
    data["alpha"] = 5;
    data["beta"] = 3;
    engine.load(std::move(data));

    auto result = engine.suggest("al");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "alpha");
}

TEST(GhostTextTest, RecordAndFrequencies) {
    GhostTextEngine engine;
    engine.record("foo");
    engine.record("foo");
    engine.record("bar");

    const auto& freq = engine.frequencies();
    EXPECT_EQ(freq.at("foo"), 2);
    EXPECT_EQ(freq.at("bar"), 1);
}

TEST(GhostTextTest, ClearResetsAll) {
    GhostTextEngine engine;
    engine.record("something");
    engine.clear();
    EXPECT_TRUE(engine.frequencies().empty());
    EXPECT_FALSE(engine.suggest("some").has_value());
}

TEST(GhostTextTest, RecordEmptyStringIgnored) {
    GhostTextEngine engine;
    engine.record("");
    EXPECT_TRUE(engine.frequencies().empty());
}

} // namespace
} // namespace euxis::etx

// NOLINTEND(bugprone-unchecked-optional-access)
