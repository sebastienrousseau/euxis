#include <gtest/gtest.h>

#include "euxis/sca/scanner.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace euxis::sca {
namespace {

namespace fs = std::filesystem;

struct TmpDir {
    fs::path path;
    TmpDir() {
        path = fs::temp_directory_path() / ("euxis-sca-test-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }
    ~TmpDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write(const fs::path& p, const std::string& contents) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << contents;
}

TEST(Scanner, FindsCargoLockAndPackageLockInSameTree) {
    TmpDir dir;
    write(dir.path / "Cargo.lock", R"toml(
[[package]]
name = "serde"
version = "1.0.197"
source = "registry+x"
)toml");
    write(dir.path / "frontend" / "package-lock.json", R"toml({
  "name": "fe",
  "version": "1.0.0",
  "lockfileVersion": 3,
  "packages": {
    "": {"name": "fe", "version": "1.0.0"},
    "node_modules/lodash": {"version": "4.17.21"}
  }
})toml");
    auto res = scan_directory(dir.path);
    ASSERT_TRUE(res.has_value()) << res.error().message;
    EXPECT_EQ(res->manifests.size(), 2U);
}

TEST(Scanner, SkipsIgnoredDirectories) {
    TmpDir dir;
    write(dir.path / "node_modules" / "Cargo.lock", R"toml(
[[package]]
name = "ignored"
version = "1"
source = "x"
)toml");
    write(dir.path / "Cargo.lock", R"toml(
[[package]]
name = "kept"
version = "1"
source = "x"
)toml");
    auto res = scan_directory(dir.path);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->manifests.size(), 1U);
    EXPECT_EQ(res->manifests[0].entries[0].name, "kept");
}

TEST(Scanner, ReturnsErrorForMissingRoot) {
    TmpDir dir;
    fs::remove(dir.path);
    auto res = scan_directory(dir.path);
    EXPECT_FALSE(res.has_value());
}

TEST(Scanner, SinglefileTargetDispatchesDirectly) {
    TmpDir dir;
    write(dir.path / "go.sum", "rsc.io/quote v3.1.0 h1:ABC==\n");
    auto res = scan_directory(dir.path / "go.sum");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->manifests.size(), 1U);
    EXPECT_EQ(res->manifests[0].ecosystem, Ecosystem::Golang);
}

TEST(Scanner, ToSbomDocumentDedupesByPurl) {
    TmpDir dir;
    write(dir.path / "a" / "Cargo.lock", R"toml(
[[package]]
name = "shared"
version = "1.0.0"
source = "registry+x"
)toml");
    write(dir.path / "b" / "Cargo.lock", R"toml(
[[package]]
name = "shared"
version = "1.0.0"
source = "registry+x"
)toml");
    auto res = scan_directory(dir.path);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->manifests.size(), 2U);

    auto doc = to_sbom_document(*res, dir.path);
    ASSERT_TRUE(doc.root.has_value());
    EXPECT_EQ(doc.components.size(), 1U);  // shared@1.0.0 dedup'd
}

TEST(Scanner, ToSbomDocumentBuildsDepGraph) {
    TmpDir dir;
    write(dir.path / "Cargo.lock", R"toml(
[[package]]
name = "a"
version = "1"
source = "registry+x"
dependencies = ["b 1 (registry+x)"]

[[package]]
name = "b"
version = "1"
source = "registry+x"
)toml");
    auto res = scan_directory(dir.path);
    ASSERT_TRUE(res.has_value());
    auto doc = to_sbom_document(*res, dir.path);
    EXPECT_EQ(doc.dependencies.size(), 1U);
    EXPECT_EQ(doc.dependencies[0].ref,           "pkg:cargo/a@1");
    ASSERT_EQ(doc.dependencies[0].depends_on.size(), 1U);
    EXPECT_EQ(doc.dependencies[0].depends_on[0], "pkg:cargo/b@1");
}

TEST(Scanner, ToSbomDocumentSetsRootProperties) {
    TmpDir dir;
    write(dir.path / "Cargo.lock", R"toml(
[[package]]
name = "a"
version = "1"
source = "registry+x"
)toml");
    auto res = scan_directory(dir.path);
    ASSERT_TRUE(res.has_value());
    auto doc = to_sbom_document(*res, dir.path);
    EXPECT_EQ(doc.properties["euxis:manifest-count"], "1");
    EXPECT_FALSE(doc.properties["euxis:scan-root"].empty());
}

} // namespace
} // namespace euxis::sca
