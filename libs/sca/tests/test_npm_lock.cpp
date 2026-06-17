#include <gtest/gtest.h>

#include "euxis/sca/npm_lock.hpp"

namespace euxis::sca {
namespace {

TEST(NpmLock, ParsesV3WithRootAndDeps) {
    std::string contents = R"({
  "name": "my-app",
  "version": "1.0.0",
  "lockfileVersion": 3,
  "requires": true,
  "packages": {
    "": {
      "name": "my-app",
      "version": "1.0.0",
      "dependencies": {
        "lodash": "^4.17.0"
      },
      "devDependencies": {
        "jest": "^29.0.0"
      }
    },
    "node_modules/lodash": {
      "version": "4.17.21",
      "resolved": "https://registry.npmjs.org/lodash/-/lodash-4.17.21.tgz",
      "integrity": "sha512-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    },
    "node_modules/jest": {
      "version": "29.7.0",
      "dev": true
    }
  }
})";
    auto p = parse_npm_lock(contents);
    ASSERT_TRUE(p.has_value()) << (p ? "" : p.error().message);
    ASSERT_TRUE(p->root.has_value());
    EXPECT_EQ(p->root->name, "my-app");
    ASSERT_EQ(p->entries.size(), 2U);

    const ManifestEntry* lodash = nullptr;
    const ManifestEntry* jest   = nullptr;
    for (const auto& e : p->entries) {
        if (e.name == "lodash") lodash = &e;
        if (e.name == "jest")   jest   = &e;
    }
    ASSERT_NE(lodash, nullptr);
    ASSERT_NE(jest,   nullptr);
    EXPECT_EQ(lodash->version, "4.17.21");
    EXPECT_TRUE(lodash->is_direct);
    EXPECT_FALSE(lodash->is_dev);
    EXPECT_TRUE(jest->is_dev);
    EXPECT_TRUE(jest->is_direct);
    ASSERT_TRUE(lodash->hash.has_value());
    EXPECT_EQ(lodash->hash->algorithm, euxis::sbom::HashAlgorithm::Sha512);
}

TEST(NpmLock, ParsesScopedPackages) {
    std::string contents = R"({
  "name": "x",
  "version": "1",
  "lockfileVersion": 3,
  "packages": {
    "": {"name": "x", "version": "1"},
    "node_modules/@babel/core": {"version": "7.24.0"}
  }
})";
    auto p = parse_npm_lock(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->entries.size(), 1U);
    EXPECT_EQ(p->entries[0].ns,      "@babel");
    EXPECT_EQ(p->entries[0].name,    "core");
    EXPECT_EQ(p->entries[0].version, "7.24.0");
}

TEST(NpmLock, RejectsV1) {
    std::string contents = R"({
  "lockfileVersion": 1,
  "packages": {}
})";
    EXPECT_FALSE(parse_npm_lock(contents).has_value());
}

TEST(NpmLock, EmptyPackagesFails) {
    std::string contents = R"({
  "lockfileVersion": 3,
  "packages": {}
})";
    EXPECT_FALSE(parse_npm_lock(contents).has_value());
}

TEST(NpmLock, EcosystemSet) {
    std::string contents = R"({
  "lockfileVersion": 3,
  "packages": {
    "": {"name": "x", "version": "1"},
    "node_modules/y": {"version": "2"}
  }
})";
    auto p = parse_npm_lock(contents);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->ecosystem, Ecosystem::Npm);
}

TEST(NpmLock, NestedNodeModulesUsesLastSegment) {
    // "node_modules/x/node_modules/y" → name=y (the deduplicated copy)
    std::string contents = R"({
  "lockfileVersion": 3,
  "packages": {
    "": {"name": "root", "version": "1"},
    "node_modules/x/node_modules/y": {"version": "0.9.0"}
  }
})";
    auto p = parse_npm_lock(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->entries.size(), 1U);
    EXPECT_EQ(p->entries[0].name,    "y");
    EXPECT_EQ(p->entries[0].version, "0.9.0");
}

} // namespace
} // namespace euxis::sca
