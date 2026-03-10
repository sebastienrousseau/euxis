#include "euxis/cli/cli_provider.hpp"

namespace euxis::cli {

CliProvider::CliProvider(const std::string& data_dir)
    : router_(data_dir)
    , executor_(data_dir)
    , auth_store_(data_dir)
{
}

auto CliProvider::route(const std::string& agent_tier,
                        const std::string& task) -> runtime::ModelSpec {
    auto selection = router_.route(agent_tier, task);
    return runtime::ModelSpec{
        .provider = selection.provider,
        .model = selection.model,
        .tier = tier_label(selection.tier),
        .estimated_cost_per_1m = selection.estimated_cost_per_1m,
        .parameters = nlohmann::json::object()
    };
}

auto CliProvider::execute(const std::string& model,
                          const std::string& prompt,
                          int timeout_ms,
                          const std::optional<std::vector<std::string>>&,
                          std::function<void(const std::string&)>) -> runtime::ProviderResponse {
    
    // Create a minimal selection based on model name
    ModelSelection selection{
        .provider = "auto",
        .model = model,
        .tier = Tier::Code,
        .estimated_cost_per_1m = 0.0,
    };

    auto auth = auth_store_.resolve(selection.provider);
    auto response = executor_.execute(selection, prompt, timeout_ms / 1000,
                                       auth ? std::optional{*auth} : std::nullopt);

    return runtime::ProviderResponse{
        .success = response.success,
        .output = response.output,
        .error = response.error,
        .input_tokens = 0,
        .output_tokens = 0,
        .duration_ms = response.duration_ms
    };
}

auto CliProvider::execute(const runtime::ModelSpec& spec,
                          const std::string& prompt,
                          int timeout_ms,
                          const std::optional<std::vector<std::string>>&,
                          std::function<void(const std::string&)>) -> runtime::ProviderResponse {
    ModelSelection selection{
        .provider = spec.provider,
        .model = spec.model,
        .tier = parse_tier(spec.tier),
        .estimated_cost_per_1m = spec.estimated_cost_per_1m,
    };

    auto auth = auth_store_.resolve(spec.provider);
    auto response = executor_.execute(selection, prompt, timeout_ms / 1000,
                                       auth ? std::optional{*auth} : std::nullopt);

    return runtime::ProviderResponse{
        .success = response.success,
        .output = response.output,
        .error = response.error,
        .input_tokens = 0,
        .output_tokens = 0,
        .duration_ms = response.duration_ms
    };
}

auto CliProvider::switch_model(const runtime::ModelSpec&) -> bool {
    // CliProvider routes dynamically — switching is a no-op
    return true;
}

} // namespace euxis::cli
