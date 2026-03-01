#pragma once

#include <chrono>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::identity {

/// A DID verification method following W3C DID Core specification.
struct VerificationMethod {
    std::string id;
    std::string type = "Ed25519VerificationKey2020";
    std::string controller;
    std::string public_key_multibase;
};

/// A service endpoint attached to a DID document.
struct ServiceEndpoint {
    std::string id;
    std::string type;
    std::string service_endpoint;
};

/// A W3C DID Document representing an agent's decentralised identity.
struct DIDDocument {
    std::string id;  // "did:euxis:{agent_id}"
    std::string context = "https://www.w3.org/ns/did/v1";
    std::vector<VerificationMethod> verification_methods;
    std::vector<std::string> authentication;
    std::vector<ServiceEndpoint> services;
    std::string created;
    std::string updated;

    /// Serialise the DID document to a JSON object.
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Create a DID string for the given agent ID.
/// Returns "did:euxis:{agent_id}".
[[nodiscard]] auto create_did(std::string_view agent_id) -> std::string;

/// Create a full DID document for an agent, including a verification method
/// derived from the given Ed25519 public key and optional service endpoints.
[[nodiscard]] auto create_did_document(
    std::string_view agent_id,
    std::span<const std::byte, 32> public_key,
    std::vector<ServiceEndpoint> services = {}
) -> DIDDocument;

/// Parse and resolve a DID string of the form "did:method:id".
/// Returns {method, id} on success, or an error description.
[[nodiscard]] auto resolve_did(std::string_view did)
    -> std::expected<std::pair<std::string, std::string>, std::string>;

} // namespace euxis::identity
