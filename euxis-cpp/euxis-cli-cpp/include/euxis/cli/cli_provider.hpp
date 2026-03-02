#pragma once

#include <euxis/runtime/provider.hpp>
#include <euxis/cli/provider_router.hpp>
#include <euxis/cli/provider_executor.hpp>
#include <euxis/cli/auth_profile_store.hpp>

#include <string>

namespace euxis::cli {

/// Adapter implementing runtime::IProvider by wrapping ProviderRouter + ProviderExecutor.
class CliProvider : public runtime::IProvider {
public:
    explicit CliProvider(const std::string& data_dir);

    auto route(const std::string& agent_tier,
               const std::string& task) -> runtime::ModelSpec override;

    auto execute(const runtime::ModelSpec& spec,
                 const std::string& prompt,
                 int timeout_seconds = 120) -> runtime::ProviderResult override;

    auto switch_model(const runtime::ModelSpec& new_spec) -> bool override;

private:
    ProviderRouter router_;
    ProviderExecutor executor_;
    AuthProfileStore auth_store_;
};

} // namespace euxis::cli
