/// @file
/// @brief Decentralized Identifier (DID) support for agent identities.
#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>
#include <span>

#include <nlohmann/json.hpp>

namespace euxis::identity {

/// @brief A service endpoint associated with a DID.
struct ServiceEndpoint {
    std::string id;
    std::string type;
    std::string service_endpoint;
};

/// @brief A verification method (e.g. public key) in a DID document.
struct VerificationMethod {
    std::string id;
    std::string type;
    std::string controller;
    std::string public_key_multibase;
};

/// @brief A full W3C-compliant DID Document.
struct DIDDocument {
    std::string id;
    std::string context;
    std::vector<VerificationMethod> verification_methods;
    std::vector<std::string> authentication;
    std::vector<ServiceEndpoint> services;
    std::string created;
    std::string updated;

    [[nodiscard]] auto to_json() const -> nlohmann::json;
};

/// @brief Generate a DID string from an agent ID.
[[nodiscard]] auto create_did(std::string_view agent_id) -> std::string;

/// @brief Generate a full DID document for an agent.
[[nodiscard]] auto create_did_document(
    std::string_view agent_id,
    std::span<const std::byte, 32> public_key,
    std::vector<ServiceEndpoint> services = {}
) -> DIDDocument;

/// @brief Resolve a DID to its method and identifier parts.
[[nodiscard]] auto resolve_did(std::string_view did)
    -> std::expected<std::pair<std::string, std::string>, std::string>;

} // namespace euxis::identity
