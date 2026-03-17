#include "euxis/identity/registry.hpp"

#include <spdlog/spdlog.h>

namespace euxis::identity {

auto InMemoryIdentityRegistry::register_agent(AgentIdentity identity) -> bool {
    const auto& did = identity.did;
    if (agents_.contains(did)) {
        spdlog::warn("Agent with DID {} is already registered", did);
        return false;
    }
    spdlog::info("Registering agent with DID {}", did);
    agents_.emplace(did, std::move(identity));
    return true;
}

auto InMemoryIdentityRegistry::resolve(std::string_view did) -> std::optional<AgentIdentity> {
    const auto it = agents_.find(std::string{did});
    if (it == agents_.end()) {
        spdlog::debug("DID {} not found in registry", did);
        return std::nullopt;
    }
    return it->second;
}

auto InMemoryIdentityRegistry::list_agents() -> std::vector<AgentIdentity> {
    std::vector<AgentIdentity> result;
    result.reserve(agents_.size());
    for (const auto& [_, identity] : agents_) {
        result.push_back(identity);
    }
    return result;
}

auto InMemoryIdentityRegistry::revoke(std::string_view did) -> bool {
    const auto it = agents_.find(std::string{did});
    if (it == agents_.end()) {
        spdlog::warn("Cannot revoke DID {}: not found", did);
        return false;
    }
    spdlog::info("Revoking agent with DID {}", did);
    agents_.erase(it);
    return true;
}

} // namespace euxis::identity
