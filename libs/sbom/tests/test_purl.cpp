#include <gtest/gtest.h>

#include "euxis/sbom/purl.hpp"

namespace euxis::sbom {
namespace {

TEST(PurlBuild, NpmSimple) {
    auto p = Purl::build(PurlType::Npm, "lodash", "4.17.21");
    EXPECT_EQ(p.to_string(), "pkg:npm/lodash@4.17.21");
}

TEST(PurlBuild, NpmScoped) {
    auto p = Purl::build(PurlType::Npm, "core", "1.0.0", "@babel");
    EXPECT_EQ(p.to_string(), "pkg:npm/%40babel/core@1.0.0");
}

TEST(PurlBuild, CargoSimple) {
    auto p = Purl::build(PurlType::Cargo, "serde", "1.0.197");
    EXPECT_EQ(p.to_string(), "pkg:cargo/serde@1.0.197");
}

TEST(PurlBuild, GolangNestedNamespace) {
    auto p = Purl::build(PurlType::Golang, "client-go", "v0.32.1", "k8s.io");
    EXPECT_EQ(p.to_string(), "pkg:golang/k8s.io/client-go@v0.32.1");
}

TEST(PurlBuild, MavenGroupId) {
    auto p = Purl::build(PurlType::Maven, "commons-lang3", "3.14.0", "org.apache.commons");
    EXPECT_EQ(p.to_string(), "pkg:maven/org.apache.commons/commons-lang3@3.14.0");
}

TEST(PurlBuild, PypiVersionWithDots) {
    auto p = Purl::build(PurlType::Pypi, "django", "5.0.4");
    EXPECT_EQ(p.to_string(), "pkg:pypi/django@5.0.4");
}

TEST(PurlBuild, TypeIsLowercased) {
    Purl p;
    p.type = "NPM";
    p.name = "react";
    p.version = "19.0.0";
    EXPECT_EQ(p.to_string(), "pkg:npm/react@19.0.0");
}

TEST(PurlBuild, QualifiersAreSortedAndLowercased) {
    Purl p;
    p.type = "generic";
    p.name = "blob";
    p.version = "1";
    p.qualifiers["Repository_URL"] = "https://example.com";
    p.qualifiers["arch"] = "amd64";
    auto s = p.to_string();
    auto q_arch = s.find("arch=amd64");
    auto q_repo = s.find("repository_url=https%3A%2F%2Fexample.com");
    ASSERT_NE(q_arch, std::string::npos);
    ASSERT_NE(q_repo, std::string::npos);
    EXPECT_LT(q_arch, q_repo);
}

TEST(PurlBuild, SubpathAppended) {
    Purl p;
    p.type = "github";
    p.ns = "sebastienrousseau";
    p.name = "euxis";
    p.version = "v0.1.2";
    p.subpath = "libs/sbom";
    EXPECT_EQ(p.to_string(), "pkg:github/sebastienrousseau/euxis@v0.1.2#libs/sbom");
}

TEST(PercentEncode, UnreservedPassThrough) {
    EXPECT_EQ(percent_encode_segment("Hello-World_1.0~ok"), "Hello-World_1.0~ok");
}

TEST(PercentEncode, EncodesReserved) {
    EXPECT_EQ(percent_encode_segment("a/b"),   "a%2Fb");
    EXPECT_EQ(percent_encode_segment("a b"),   "a%20b");
    EXPECT_EQ(percent_encode_segment("foo@1"), "foo%401");
}

TEST(PercentDecode, RoundTripsAscii) {
    auto enc = percent_encode_segment("hello world");
    EXPECT_EQ(percent_decode_segment(enc), "hello world");
}

TEST(PercentDecode, RejectsTruncated) {
    // Truncated should pass through the literal % character.
    EXPECT_EQ(percent_decode_segment("a%"), "a%");
}

TEST(PurlParse, NpmSimple) {
    auto p = parse_purl("pkg:npm/lodash@4.17.21");
    ASSERT_TRUE(p.has_value()) << (p ? "" : p.error().message);
    EXPECT_EQ(p->type, "npm");
    EXPECT_TRUE(p->ns.empty());
    EXPECT_EQ(p->name, "lodash");
    EXPECT_EQ(p->version, "4.17.21");
}

TEST(PurlParse, NpmScoped) {
    auto p = parse_purl("pkg:npm/%40babel/core@1.0.0");
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->ns, "@babel");
    EXPECT_EQ(p->name, "core");
    EXPECT_EQ(p->version, "1.0.0");
}

TEST(PurlParse, RoundTripCargo) {
    std::string canonical = "pkg:cargo/serde@1.0.197";
    auto p = parse_purl(canonical);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->to_string(), canonical);
}

TEST(PurlParse, RoundTripGolang) {
    std::string canonical = "pkg:golang/k8s.io/client-go@v0.32.1";
    auto p = parse_purl(canonical);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->to_string(), canonical);
}

TEST(PurlParse, QualifiersAndSubpath) {
    auto p = parse_purl("pkg:generic/blob@1?arch=amd64&os=linux#path/to/binary");
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->qualifiers["arch"], "amd64");
    EXPECT_EQ(p->qualifiers["os"],   "linux");
    EXPECT_EQ(p->subpath, "path/to/binary");
}

TEST(PurlParse, RejectsMissingScheme) {
    EXPECT_FALSE(parse_purl("npm/lodash@1").has_value());
}

TEST(PurlParse, RejectsMissingName) {
    EXPECT_FALSE(parse_purl("pkg:npm/").has_value());
}

} // namespace
} // namespace euxis::sbom
