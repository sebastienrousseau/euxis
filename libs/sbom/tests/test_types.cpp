#include <gtest/gtest.h>

#include "euxis/sbom/types.hpp"

#include <regex>

namespace euxis::sbom {
namespace {

TEST(ComponentType, AllStringsKnown) {
    EXPECT_STREQ(component_type_str(ComponentType::Library),              "library");
    EXPECT_STREQ(component_type_str(ComponentType::Application),          "application");
    EXPECT_STREQ(component_type_str(ComponentType::Framework),            "framework");
    EXPECT_STREQ(component_type_str(ComponentType::OperatingSystem),      "operating-system");
    EXPECT_STREQ(component_type_str(ComponentType::MachineLearningModel), "machine-learning-model");
    EXPECT_STREQ(component_type_str(ComponentType::Data),                 "data");
}

TEST(HashAlgorithm, CycloneDxVocab) {
    EXPECT_STREQ(hash_alg_cyclonedx(HashAlgorithm::Sha256),      "SHA-256");
    EXPECT_STREQ(hash_alg_cyclonedx(HashAlgorithm::Blake2b_512), "BLAKE2b-512");
    EXPECT_STREQ(hash_alg_cyclonedx(HashAlgorithm::Blake3),      "BLAKE3");
}

TEST(HashAlgorithm, SpdxVocab) {
    EXPECT_STREQ(hash_alg_spdx(HashAlgorithm::Sha256), "SHA256");
    EXPECT_STREQ(hash_alg_spdx(HashAlgorithm::Sha512), "SHA512");
    EXPECT_STREQ(hash_alg_spdx(HashAlgorithm::Md5),    "MD5");
}

TEST(DigestLength, ExpectedLengths) {
    EXPECT_EQ(digest_hex_length(HashAlgorithm::Md5),         32U);
    EXPECT_EQ(digest_hex_length(HashAlgorithm::Sha1),        40U);
    EXPECT_EQ(digest_hex_length(HashAlgorithm::Sha256),      64U);
    EXPECT_EQ(digest_hex_length(HashAlgorithm::Sha384),      96U);
    EXPECT_EQ(digest_hex_length(HashAlgorithm::Sha512),     128U);
    EXPECT_EQ(digest_hex_length(HashAlgorithm::Blake2b_256), 64U);
    EXPECT_EQ(digest_hex_length(HashAlgorithm::Blake3),       0U);  // variable
}

TEST(IsValidHash, RejectsWrongLength) {
    Hash h{HashAlgorithm::Sha256, "abc"};
    EXPECT_FALSE(is_valid_hash(h));
}

TEST(IsValidHash, RejectsNonHex) {
    Hash h{HashAlgorithm::Sha256, std::string(64, 'z')};
    EXPECT_FALSE(is_valid_hash(h));
}

TEST(IsValidHash, AcceptsCorrectShape) {
    Hash h{HashAlgorithm::Sha256, std::string(64, 'a')};
    EXPECT_TRUE(is_valid_hash(h));
}

TEST(IsValidHash, AcceptsAnyLengthForBlake3) {
    Hash h{HashAlgorithm::Blake3, std::string(50, 'a')};
    EXPECT_TRUE(is_valid_hash(h));
}

TEST(IsValidHash, RejectsEmpty) {
    EXPECT_FALSE(is_valid_hash(Hash{HashAlgorithm::Sha256, ""}));
}

TEST(ExternalRef, KnownVocab) {
    EXPECT_STREQ(external_ref_type_str(ExternalRefType::Vcs),         "vcs");
    EXPECT_STREQ(external_ref_type_str(ExternalRefType::Advisories),  "advisories");
    EXPECT_STREQ(external_ref_type_str(ExternalRefType::BuildMeta),   "build-meta");
}

TEST(SerialNumber, IsUrnUuidShape) {
    auto sn = generate_serial_number();
    // urn:uuid:8-4-4-4-12
    std::regex shape{R"(urn:uuid:[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12})"};
    EXPECT_TRUE(std::regex_match(sn, shape)) << "Got " << sn;
}

TEST(SerialNumber, IsUnique) {
    auto a = generate_serial_number();
    auto b = generate_serial_number();
    EXPECT_NE(a, b);
}

TEST(Rfc3339, EmitsZuluFormat) {
    auto epoch = std::chrono::system_clock::from_time_t(0);
    EXPECT_EQ(to_rfc3339(epoch), "1970-01-01T00:00:00Z");
}

} // namespace
} // namespace euxis::sbom
