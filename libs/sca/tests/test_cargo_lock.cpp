#include <gtest/gtest.h>

#include "euxis/sca/cargo_lock.hpp"

namespace euxis::sca {
namespace {

TEST(CargoLock, ParsesSinglePackage) {
    std::string contents = R"(# auto-generated
version = 3

[[package]]
name = "serde"
version = "1.0.197"
source = "registry+https://github.com/rust-lang/crates.io-index"
checksum = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
dependencies = [
    "serde_derive 1.0.197 (registry+https://github.com/rust-lang/crates.io-index)",
]
)";
    auto p = parse_cargo_lock(contents);
    ASSERT_TRUE(p.has_value()) << (p ? "" : p.error().message);
    ASSERT_EQ(p->entries.size(), 1U);
    EXPECT_EQ(p->entries[0].name, "serde");
    EXPECT_EQ(p->entries[0].version, "1.0.197");
    EXPECT_TRUE(p->entries[0].source.starts_with("registry+"));
    ASSERT_TRUE(p->entries[0].hash.has_value());
    EXPECT_EQ(p->entries[0].hash->algorithm, euxis::sbom::HashAlgorithm::Sha256);
    ASSERT_EQ(p->entries[0].depends_on.size(), 1U);
    EXPECT_EQ(p->entries[0].depends_on[0], "serde_derive");
}

TEST(CargoLock, ParsesMultiplePackages) {
    std::string contents = R"(
[[package]]
name = "tokio"
version = "1.0.0"
source = "registry+x"

[[package]]
name = "futures"
version = "0.3.0"
source = "registry+x"
)";
    auto p = parse_cargo_lock(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->entries.size(), 2U);
    EXPECT_EQ(p->entries[0].name, "tokio");
    EXPECT_EQ(p->entries[1].name, "futures");
}

TEST(CargoLock, DetectsWorkspaceRootBySourceAbsence) {
    std::string contents = R"(
[[package]]
name = "my-workspace"
version = "0.1.0"
dependencies = []

[[package]]
name = "external"
version = "1.0.0"
source = "registry+x"
)";
    auto p = parse_cargo_lock(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_TRUE(p->root.has_value());
    EXPECT_EQ(p->root->name, "my-workspace");
}

TEST(CargoLock, IgnoresMetadataSections) {
    std::string contents = R"(
[metadata]
"checksum foo 1.0 (registry+x)" = "deadbeef"

[[package]]
name = "ok"
version = "1.0.0"
source = "registry+x"
)";
    auto p = parse_cargo_lock(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->entries.size(), 1U);
    EXPECT_EQ(p->entries[0].name, "ok");
}

TEST(CargoLock, SingleLineDependenciesArray) {
    std::string contents = R"(
[[package]]
name = "a"
version = "1.0.0"
source = "registry+x"
dependencies = ["b 1.0.0 (registry+x)", "c 2.0.0 (registry+x)"]
)";
    auto p = parse_cargo_lock(contents);
    ASSERT_TRUE(p.has_value());
    ASSERT_EQ(p->entries.size(), 1U);
    ASSERT_EQ(p->entries[0].depends_on.size(), 2U);
    EXPECT_EQ(p->entries[0].depends_on[0], "b");
    EXPECT_EQ(p->entries[0].depends_on[1], "c");
}

TEST(CargoLock, EmptyContentsFails) {
    auto p = parse_cargo_lock("");
    EXPECT_FALSE(p.has_value());
}

TEST(CargoLock, EcosystemSet) {
    std::string contents = R"(
[[package]]
name = "x"
version = "1"
source = "registry+x"
)";
    auto p = parse_cargo_lock(contents);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->ecosystem, Ecosystem::Cargo);
}

} // namespace
} // namespace euxis::sca
