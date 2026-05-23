#include "euxis/cli/cli_provider.hpp"

namespace euxis::cli {

namespace {

/// Infer the provider from a model name prefix.
auto infer_provider(const std::string& model) -> std::string {
    if (model.starts_with("claude") || model.starts_with("anthropic")) return "claude";
    if (model.starts_with("gpt") || model.starts_with("o1") || model.starts_with("codex") || model.starts_with("o3") || model.starts_with("o4")) return "openai";
    if (model.starts_with("gemini")) return "gemini";
    if (model.starts_with("qwen") || model.starts_with("llama") || model.starts_with("mistral") || model.starts_with("deepseek")) return "ollama";
    return "claude"; // safe default
}

} // namespace

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
        .provider = infer_provider(model),
        .model = model,
        .tier = Tier::Code,
        .estimated_cost_per_1m = 0.0,
        .route_reason = {},
        .task_class = {},
    };

    auto auth = auth_store_.resolve(selection.provider);
    auto response = executor_.execute(selection, prompt, timeout_ms / 1000, auth);

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
        .route_reason = {},
        .task_class = {},
    };

    auto auth = auth_store_.resolve(spec.provider);
    auto response = executor_.execute(selection, prompt, timeout_ms / 1000, auth);

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
