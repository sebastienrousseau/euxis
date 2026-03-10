/// @file
/// @brief Registry for managing agent identities and credentials.
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "did.hpp"
#include "credentials.hpp"
#include "attestation.hpp"

namespace euxis::identity {

/// @brief Unified identity record for an agent in the registry.
struct AgentIdentity {
    std::string did;
    std::string name;
    std::string role;
    std::string public_key;
    std::vector<VerifiableCredential> credentials;
    std::vector<Attestation> attestations;
    std::string created_at;
    nlohmann::json metadata;
};

/// @brief Abstract interface for identity persistence and resolution.
class IdentityRegistry {
public:
    virtual ~IdentityRegistry() = default;
    
    /// @brief Store a new agent identity.
    virtual auto register_agent(AgentIdentity identity) -> bool = 0;
    
    /// @brief resolve an identity by its DID.
    virtual auto resolve(std::string_view did) -> std::optional<AgentIdentity> = 0;
    
    /// @brief List all identities in the registry.
    virtual auto list_agents() -> std::vector<AgentIdentity> = 0;
    
    /// @brief Remove an identity from the registry.
    virtual auto revoke(std::string_view did) -> bool = 0;
};

/// @brief volatile in-memory implementation of the identity registry.
class InMemoryIdentityRegistry : public IdentityRegistry {
public:
    auto register_agent(AgentIdentity identity) -> bool override;
    auto resolve(std::string_view did) -> std::optional<AgentIdentity> override;
    auto list_agents() -> std::vector<AgentIdentity> override;
    auto revoke(std::string_view did) -> bool override;

private:
    std::map<std::string, AgentIdentity> agents_;
};

} // namespace euxis::identity
