#pragma once

#include "euxis/runtime/provider.hpp"
#include "euxis/runtime/agent_session.hpp"
#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/auth_profile_store.hpp"

#include <string>

namespace euxis::cli {

/**
 * @brief Main CLI implementation of the runtime Provider interface.
 * 
 * Orchestrates routing and execution by delegating to the router, 
 * auth store, and executor modules.
 */
class CliProvider : public runtime::IProvider {
public:
    explicit CliProvider(const std::string& data_dir);

    auto route(const std::string& agent_tier,
               const std::string& task) -> runtime::ModelSpec override;

    // Implementation of the virtual string-based execute
    auto execute(const std::string& model,
                 const std::string& prompt,
                 int timeout_ms = 30000,
                 const std::optional<std::vector<std::string>>& stop_sequences = std::nullopt,
                 std::function<void(const std::string&)> stream_callback = nullptr) -> runtime::ProviderResponse override;

    // Implementation of the virtual ModelSpec-based execute
    auto execute(const runtime::ModelSpec& spec,
                 const std::string& prompt,
                 int timeout_ms = 30000,
                 const std::optional<std::vector<std::string>>& stop_sequences = std::nullopt,
                 std::function<void(const std::string&)> stream_callback = nullptr) -> runtime::ProviderResponse override;

    auto switch_model(const runtime::ModelSpec& new_spec) -> bool override;

private:
    ProviderRouter router_;
    ProviderExecutor executor_;
    AuthProfileStore auth_store_;
};

} // namespace euxis::cli
