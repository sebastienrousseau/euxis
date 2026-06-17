#include <gtest/gtest.h>

#include "euxis/sca/go_sum.hpp"

namespace euxis::sca {
namespace {

TEST(GoSum, DedupesZipAndGoModEntries) {
    std::string contents =
        "github.com/foo/bar v1.0.0 h1:ABC123==\n"
        "github.com/foo/bar v1.0.0/go.mod h1:DEF456==\n";
    auto p = parse_go_sum(contents);
    ASSERT_TRUE(p.has_value()) << (p ? "" : p.error().message);
    ASSERT_EQ(p->entries.size(), 1U);
    EXPECT_EQ(p->entries[0].name, "bar");
    EXPECT_EQ(p->entries[0].ns,   "github.com/foo");
    EXPECT_EQ(p->entries[0].version, "v1.0.0");
    ASSERT_TRUE(p->entries[0].hash.has_value());
    EXPECT_EQ(p->entries[0].hash->value, "ABC123==");
}

TEST(GoSum, PrefersZipHashOverGoMod) {
    // go.mod arrives first
    std::string contents =
        "github.com/foo/bar v1.0.0/go.mod h1:DEF456==\n"
        "github.com/foo/bar v1.0.0 h1:ABC123==\n";
    auto p = parse_go_sum(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->entries.size(), 1U);
    EXPECT_EQ(p->entries[0].hash->value, "ABC123==");
}

TEST(GoSum, SplitsNamespaceCorrectly) {
    std::string contents =
        "k8s.io/client-go v0.32.1 h1:ABC==\n"
        "rsc.io/quote v3.1.0 h1:DEF==\n"
        "go.uber.org/zap v1.27.0 h1:GHI==\n";
    auto p = parse_go_sum(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->entries.size(), 3U);
    EXPECT_EQ(p->entries[0].ns,   "k8s.io");
    EXPECT_EQ(p->entries[0].name, "client-go");
    EXPECT_EQ(p->entries[1].ns,   "rsc.io");
    EXPECT_EQ(p->entries[1].name, "quote");
    EXPECT_EQ(p->entries[2].ns,   "go.uber.org");
    EXPECT_EQ(p->entries[2].name, "zap");
}

TEST(GoSum, SkipsBlankAndCommentLines) {
    std::string contents =
        "# this is a comment\n"
        "\n"
        "rsc.io/quote v3.1.0 h1:ABC==\n";
    auto p = parse_go_sum(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->entries.size(), 1U);
}

TEST(GoSum, EmptyFileFails) {
    EXPECT_FALSE(parse_go_sum("").has_value());
}

TEST(GoSum, EcosystemSet) {
    auto p = parse_go_sum("rsc.io/quote v3.1.0 h1:ABC==\n");
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->ecosystem, Ecosystem::Golang);
}

TEST(GoSum, PreservesInsertionOrder) {
    std::string contents =
        "z.io/last v1.0.0 h1:Z\n"
        "a.io/first v1.0.0 h1:A\n";
    auto p = parse_go_sum(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->entries.size(), 2U);
    EXPECT_EQ(p->entries[0].name, "last");
    EXPECT_EQ(p->entries[1].name, "first");
}

} // namespace
} // namespace euxis::sca
