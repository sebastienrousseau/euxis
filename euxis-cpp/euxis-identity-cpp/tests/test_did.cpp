#include <array>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sodium.h>

#include "euxis/identity/did.hpp"

namespace euxis::identity {
namespace {

class DIDTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0);
    }
};

// --- create_did ---

TEST_F(DIDTest, CreateDIDReturnsCorrectFormat) {
    const auto did = create_did("agent-alpha");
    EXPECT_EQ(did, "did:euxis:agent-alpha");
}

TEST_F(DIDTest, CreateDIDWithNumericId) {
    const auto did = create_did("12345");
    EXPECT_EQ(did, "did:euxis:12345");
}

TEST_F(DIDTest, CreateDIDWithEmptyId) {
    const auto did = create_did("");
    EXPECT_EQ(did, "did:euxis:");
}

TEST_F(DIDTest, CreateDIDWithComplexId) {
    const auto did = create_did("org.example.agent-v2.1_beta");
    EXPECT_EQ(did, "did:euxis:org.example.agent-v2.1_beta");
}

// --- create_did_document ---

TEST_F(DIDTest, CreateDIDDocumentBasic) {
    std::array<std::byte, 32> pub_key{};
    randombytes_buf(pub_key.data(), pub_key.size());

    const auto doc = create_did_document("test-agent", pub_key);

    EXPECT_EQ(doc.id, "did:euxis:test-agent");
    EXPECT_EQ(doc.context, "https://www.w3.org/ns/did/v1");
    ASSERT_EQ(doc.verification_methods.size(), 1);
    EXPECT_EQ(doc.verification_methods[0].type, "Ed25519VerificationKey2020");
    EXPECT_EQ(doc.verification_methods[0].controller, "did:euxis:test-agent");
    EXPECT_EQ(doc.verification_methods[0].id, "did:euxis:test-agent#keys-1");
    ASSERT_EQ(doc.authentication.size(), 1);
    EXPECT_EQ(doc.authentication[0], "did:euxis:test-agent#keys-1");
    EXPECT_TRUE(doc.services.empty());
    EXPECT_FALSE(doc.created.empty());
    EXPECT_FALSE(doc.updated.empty());
    EXPECT_EQ(doc.created, doc.updated);
}

TEST_F(DIDTest, CreateDIDDocumentMultibasePrefix) {
    std::array<std::byte, 32> pub_key{};
    randombytes_buf(pub_key.data(), pub_key.size());

    const auto doc = create_did_document("multibase-check", pub_key);

    // Multibase encoding should start with 'z' prefix
    ASSERT_FALSE(doc.verification_methods.empty());
    EXPECT_TRUE(doc.verification_methods[0].public_key_multibase.starts_with("z"));
    EXPECT_GT(doc.verification_methods[0].public_key_multibase.size(), 1);
}

TEST_F(DIDTest, CreateDIDDocumentWithServices) {
    std::array<std::byte, 32> pub_key{};
    randombytes_buf(pub_key.data(), pub_key.size());

    std::vector<ServiceEndpoint> services = {
        {
            .id = "did:euxis:svc-agent#messaging",
            .type = "MessagingService",
            .service_endpoint = "https://agent.euxis.dev/messages",
        },
        {
            .id = "did:euxis:svc-agent#a2a",
            .type = "A2AProtocol",
            .service_endpoint = "https://agent.euxis.dev/a2a",
        },
    };

    const auto doc = create_did_document("svc-agent", pub_key, std::move(services));

    ASSERT_EQ(doc.services.size(), 2);
    EXPECT_EQ(doc.services[0].type, "MessagingService");
    EXPECT_EQ(doc.services[1].type, "A2AProtocol");
    EXPECT_EQ(doc.services[0].service_endpoint, "https://agent.euxis.dev/messages");
}

TEST_F(DIDTest, CreateDIDDocumentTimestampFormat) {
    std::array<std::byte, 32> pub_key{};
    randombytes_buf(pub_key.data(), pub_key.size());

    const auto doc = create_did_document("ts-agent", pub_key);

    // ISO-8601 format: YYYY-MM-DDTHH:MM:SSZ
    EXPECT_EQ(doc.created.size(), 20);
    EXPECT_EQ(doc.created[4], '-');
    EXPECT_EQ(doc.created[7], '-');
    EXPECT_EQ(doc.created[10], 'T');
    EXPECT_EQ(doc.created[13], ':');
    EXPECT_EQ(doc.created[16], ':');
    EXPECT_EQ(doc.created[19], 'Z');
}

// --- DIDDocument::to_json ---

TEST_F(DIDTest, ToJsonRoundtrip) {
    std::array<std::byte, 32> pub_key{};
    randombytes_buf(pub_key.data(), pub_key.size());

    const auto doc = create_did_document("json-agent", pub_key);
    const auto j = doc.to_json();

    EXPECT_EQ(j["@context"], "https://www.w3.org/ns/did/v1");
    EXPECT_EQ(j["id"], "did:euxis:json-agent");
    EXPECT_TRUE(j.contains("verificationMethod"));
    EXPECT_TRUE(j.contains("authentication"));
    EXPECT_TRUE(j.contains("service"));
    EXPECT_TRUE(j.contains("created"));
    EXPECT_TRUE(j.contains("updated"));

    ASSERT_EQ(j["verificationMethod"].size(), 1);
    EXPECT_EQ(j["verificationMethod"][0]["type"], "Ed25519VerificationKey2020");
    EXPECT_EQ(j["verificationMethod"][0]["controller"], "did:euxis:json-agent");
    EXPECT_TRUE(j["verificationMethod"][0]["publicKeyMultibase"].get<std::string>().starts_with("z"));
}

TEST_F(DIDTest, ToJsonWithServices) {
    std::array<std::byte, 32> pub_key{};
    randombytes_buf(pub_key.data(), pub_key.size());

    std::vector<ServiceEndpoint> services = {{
        .id = "did:euxis:j-agent#api",
        .type = "APIEndpoint",
        .service_endpoint = "https://api.euxis.dev",
    }};

    const auto doc = create_did_document("j-agent", pub_key, std::move(services));
    const auto j = doc.to_json();

    ASSERT_EQ(j["service"].size(), 1);
    EXPECT_EQ(j["service"][0]["id"], "did:euxis:j-agent#api");
    EXPECT_EQ(j["service"][0]["type"], "APIEndpoint");
    EXPECT_EQ(j["service"][0]["serviceEndpoint"], "https://api.euxis.dev");
}

TEST_F(DIDTest, ToJsonSerializesAndParses) {
    std::array<std::byte, 32> pub_key{};
    randombytes_buf(pub_key.data(), pub_key.size());

    const auto doc = create_did_document("parse-agent", pub_key);
    const auto j = doc.to_json();
    const auto serialized = j.dump();
    const auto parsed = nlohmann::json::parse(serialized);

    EXPECT_EQ(parsed["id"], "did:euxis:parse-agent");
    EXPECT_EQ(parsed, j);
}

// --- resolve_did ---

TEST_F(DIDTest, ResolveValidDID) {
    const auto result = resolve_did("did:euxis:agent-1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "euxis");
    EXPECT_EQ(result->second, "agent-1");
}

TEST_F(DIDTest, ResolveNonEuxisDID) {
    const auto result = resolve_did("did:web:example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "web");
    EXPECT_EQ(result->second, "example.com");
}

TEST_F(DIDTest, ResolveDIDWithMultipleColons) {
    const auto result = resolve_did("did:key:z6Mk:extra:parts");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "key");
    EXPECT_EQ(result->second, "z6Mk:extra:parts");
}

TEST_F(DIDTest, ResolveInvalidDIDNoPrefix) {
    const auto result = resolve_did("not-a-did");
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("must start with 'did:'") != std::string::npos);
}

TEST_F(DIDTest, ResolveInvalidDIDNoId) {
    const auto result = resolve_did("did:euxis");
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("missing method-specific id") != std::string::npos);
}

TEST_F(DIDTest, ResolveInvalidDIDEmptyMethod) {
    const auto result = resolve_did("did::some-id");
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("empty method") != std::string::npos);
}

TEST_F(DIDTest, ResolveInvalidDIDEmptyId) {
    const auto result = resolve_did("did:euxis:");
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().find("empty id") != std::string::npos);
}

} // namespace
} // namespace euxis::identity
