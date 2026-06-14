#include <gtest/gtest.h>

#include "euxis/sbom/spdx.hpp"

#include <nlohmann/json.hpp>

namespace euxis::sbom {
namespace {

SbomDocument minimal_doc_spdx() {
    SbomDocument doc;
    doc.document_namespace = "https://euxis.co/sbom";
    doc.serial_number = "test-run-001";
    doc.tools.push_back({"euxis.co", "euxis-cli", "0.1.0-test"});

    Component root;
    root.bom_ref = "self";
    root.name = "euxis-self";
    root.version = "0.1.0";
    root.type = ComponentType::Application;
    doc.root = root;

    Component dep;
    dep.bom_ref = "serde-1-0-197";
    dep.name = "serde";
    dep.version = "1.0.197";
    dep.purl = "pkg:cargo/serde@1.0.197";
    dep.licenses.push_back({"MIT", std::nullopt});
    dep.hashes.push_back({HashAlgorithm::Sha256, std::string(64, 'a')});
    doc.components.push_back(dep);

    doc.dependencies.push_back({"self", {"serde-1-0-197"}});
    return doc;
}

TEST(Spdx301, ContextRefersTo301) {
    auto json = to_spdx_3_0_1(minimal_doc_spdx());
    ASSERT_TRUE(json.contains("@context"));
    ASSERT_TRUE(json["@context"].is_array());
    EXPECT_EQ(json["@context"][0], "https://spdx.org/rdf/3.0.1/spdx-context.jsonld");
}

TEST(Spdx301, CreationInfoNodePresent) {
    auto json = to_spdx_3_0_1(minimal_doc_spdx());
    bool saw_creation = false;
    for (const auto& node : json["@graph"]) {
        if (node["@type"] == "CreationInfo") {
            saw_creation = true;
            EXPECT_EQ(node["specVersion"], "3.0.1");
            EXPECT_TRUE(node.contains("created"));
            ASSERT_TRUE(node.contains("createdUsing"));
            EXPECT_EQ(node["createdUsing"][0]["name"], "euxis-cli");
        }
    }
    EXPECT_TRUE(saw_creation);
}

TEST(Spdx301, SpdxDocumentNodePresent) {
    auto json = to_spdx_3_0_1(minimal_doc_spdx());
    bool saw_doc = false;
    for (const auto& node : json["@graph"]) {
        if (node["@type"] == "SpdxDocument") {
            saw_doc = true;
            EXPECT_EQ(node["dataLicense"], "CC0-1.0");
            EXPECT_TRUE(node["spdxId"].get<std::string>().starts_with("https://euxis.co/sbom"));
        }
    }
    EXPECT_TRUE(saw_doc);
}

TEST(Spdx301, PackageNodesUseSpdxRefPrefix) {
    auto json = to_spdx_3_0_1(minimal_doc_spdx());
    bool saw_serde = false;
    for (const auto& node : json["@graph"]) {
        if (node.value("@type", "") == "Package" && node["name"] == "serde") {
            saw_serde = true;
            EXPECT_TRUE(node["spdxId"].get<std::string>().starts_with("SPDXRef-"));
            EXPECT_EQ(node["packageVersion"], "1.0.197");
            EXPECT_EQ(node["concludedLicense"], "MIT");
        }
    }
    EXPECT_TRUE(saw_serde);
}

TEST(Spdx301, PurlEmittedAsExternalIdentifier) {
    auto json = to_spdx_3_0_1(minimal_doc_spdx());
    bool saw_purl = false;
    for (const auto& node : json["@graph"]) {
        if (node.value("@type", "") == "Package" && node.contains("externalIdentifier")) {
            for (const auto& ext : node["externalIdentifier"]) {
                if (ext["externalIdentifierType"] == "purl") {
                    saw_purl = true;
                    EXPECT_EQ(ext["identifier"], "pkg:cargo/serde@1.0.197");
                }
            }
        }
    }
    EXPECT_TRUE(saw_purl);
}

TEST(Spdx301, HashEmittedWithSpdxAlg) {
    auto json = to_spdx_3_0_1(minimal_doc_spdx());
    bool saw_hash = false;
    for (const auto& node : json["@graph"]) {
        if (node.value("@type", "") == "Package" && node.contains("verifiedUsing")) {
            for (const auto& h : node["verifiedUsing"]) {
                if (h["checksumAlgorithm"] == "SHA256") {
                    saw_hash = true;
                    EXPECT_EQ(h["checksumValue"].get<std::string>().size(), 64U);
                }
            }
        }
    }
    EXPECT_TRUE(saw_hash);
}

TEST(Spdx301, RelationshipsEmitted) {
    auto json = to_spdx_3_0_1(minimal_doc_spdx());
    bool saw_rel = false;
    for (const auto& node : json["@graph"]) {
        if (node.value("@type", "") == "Relationship") {
            saw_rel = true;
            EXPECT_EQ(node["relationshipType"], "dependsOn");
            EXPECT_EQ(node["from"], "SPDXRef-self");
            EXPECT_EQ(node["to"][0], "SPDXRef-serde-1-0-197");
        }
    }
    EXPECT_TRUE(saw_rel);
}

} // namespace
} // namespace euxis::sbom
