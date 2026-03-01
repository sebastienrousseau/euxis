#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "attestation.hpp"
#include "credentials.hpp"

namespace euxis::identity {

/// Full identity record for an agent in the registry, including DID,
/// public key, credentials, attestations, and arbitrary metadata.
struct AgentIdentity {
    std::string did;
    std::vector<std::byte> public_key;
    std::vector<VerifiableCredential> credentials;
    std::vector<Attestation> attestations;
    std::string created_at;
    nlohmann::json metadata;
};

/// Abstract identity registry interface for agent lifecycle management.
class IdentityRegistry {
public:
    virtual ~IdentityRegistry() = default;

    /// Register an agent identity. Returns false if the DID already exists.
    virtual auto register_agent(AgentIdentity identity) -> bool = 0;

    /// Resolve a DID to the full agent identity, or std::nullopt if not found.
    virtual auto resolve(std::string_view did) -> std::optional<AgentIdentity> = 0;

    /// List all registered agent identities.
    virtual auto list_agents() -> std::vector<AgentIdentity> = 0;

    /// Revoke an agent's identity by DID. Returns false if the DID was not found.
    virtual auto revoke(std::string_view did) -> bool = 0;
};

/// In-memory implementation of the identity registry backed by a hash map.
class InMemoryIdentityRegistry final : public IdentityRegistry {
public:
    auto register_agent(AgentIdentity identity) -> bool override;
    auto resolve(std::string_view did) -> std::optional<AgentIdentity> override;
    auto list_agents() -> std::vector<AgentIdentity> override;
    auto revoke(std::string_view did) -> bool override;

private:
    std::unordered_map<std::string, AgentIdentity> agents_;
};

} // namespace euxis::identity
