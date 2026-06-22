#include <gtest/gtest.h>

#include "euxis/sca/pipfile_lock.hpp"

// NOLINTBEGIN(bugprone-unchecked-optional-access) — gtest ASSERT_TRUE
// guards are invisible to clang-tidy's dataflow; tests can blanket-
// disable per docs/development/clang-tidy-policy.md.

namespace euxis::sca {
namespace {

TEST(PipfileLock, ParsesDefaultAndDevelop) {
    std::string contents = R"({
  "_meta": {"sources": [], "requires": {}, "pipfile-spec": 6},
  "default": {
    "django": {
      "version": "==5.0.4",
      "hashes": ["sha256:abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"]
    }
  },
  "develop": {
    "pytest": {
      "version": "==8.0.0",
      "hashes": ["sha256:0000000000000000000000000000000000000000000000000000000000000000"]
    }
  }
})";
    auto p = parse_pipfile_lock(contents);
    ASSERT_TRUE(p.has_value()) << (p ? "" : p.error().message);
    ASSERT_EQ(p->entries.size(), 2U);

    const ManifestEntry* django = nullptr;
    const ManifestEntry* pytest = nullptr;
    for (const auto& e : p->entries) {
        if (e.name == "django")  django = &e;
        if (e.name == "pytest")  pytest = &e;
    }
    ASSERT_NE(django, nullptr);
    ASSERT_NE(pytest, nullptr);

    EXPECT_EQ(django->version, "5.0.4");        // == stripped
    EXPECT_TRUE(django->is_direct);
    EXPECT_FALSE(django->is_dev);
    ASSERT_TRUE(django->hash.has_value());
    EXPECT_EQ(django->hash->algorithm, euxis::sbom::HashAlgorithm::Sha256);

    EXPECT_EQ(pytest->version, "8.0.0");
    EXPECT_TRUE(pytest->is_dev);
}

TEST(PipfileLock, MissingSectionsFails) {
    EXPECT_FALSE(parse_pipfile_lock("{}").has_value());
}

TEST(PipfileLock, EcosystemSet) {
    std::string contents = R"({"default": {"x": {"version": "==1"}}})";
    auto p = parse_pipfile_lock(contents);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->ecosystem, Ecosystem::Pypi);
}

TEST(PipfileLock, HandlesEmptyHashes) {
    std::string contents = R"({"default": {"x": {"version": "==1", "hashes": []}}})";
    auto p = parse_pipfile_lock(contents);
    ASSERT_TRUE(p.has_value());
    EXPECT_FALSE(p->entries[0].hash.has_value());
}

TEST(PipfileLock, SkipsUnknownHashAlgorithm) {
    std::string contents = R"({"default": {"x": {"version": "==1", "hashes": ["bcrypt:abc"]}}})";
    auto p = parse_pipfile_lock(contents);
    ASSERT_TRUE(p.has_value());
    EXPECT_FALSE(p->entries[0].hash.has_value());
}

TEST(PipfileLock, KeepsVersionVerbatimWhenNoEq) {
    std::string contents = R"({"default": {"x": {"version": ">=1.0"}}})";
    auto p = parse_pipfile_lock(contents);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->entries[0].version, ">=1.0");
}

TEST(PipfileLock, MalformedJsonReturnsError) {
    EXPECT_FALSE(parse_pipfile_lock("{not json").has_value());
}

} // namespace
} // namespace euxis::sca

// NOLINTEND(bugprone-unchecked-optional-access)
