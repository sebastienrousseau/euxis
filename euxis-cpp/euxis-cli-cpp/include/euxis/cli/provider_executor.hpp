#pragma once

#include "euxis/cli/provider_router.hpp"

#include <string>

namespace euxis::cli {

/// Result of a provider invocation.
struct ProviderResponse {
    bool success{false};
    std::string output;
    std::string error;
    int exit_code{-1};
    double duration_ms{0.0};
};

/// Invokes LLM providers via their CLI or API.
class ProviderExecutor {
public:
    explicit ProviderExecutor(const std::string& data_dir);

    /// Send a prompt to the provider specified by the model selection.
    [[nodiscard]] auto execute(const ModelSelection& selection,
                                const std::string& prompt,
                                int timeout_seconds = 120) -> ProviderResponse;

    /// Load an agent's system prompt from its prompt file (strips YAML frontmatter).
    [[nodiscard]] static auto load_agent_prompt(const std::string& euxis_home,
                                                 const std::string& prompt_path) -> std::string;

    /// Build a combined prompt from system prompt, task, and optional context.
    [[nodiscard]] static auto build_prompt(const std::string& system_prompt,
                                            const std::string& task,
                                            const std::string& context = {}) -> std::string;

private:
    std::string data_dir_;

    auto execute_claude(const std::string& model, const std::string& prompt, int timeout) -> ProviderResponse;
    auto execute_ollama(const std::string& model, const std::string& prompt, int timeout) -> ProviderResponse;
    auto execute_api(const std::string& provider, const std::string& model,
                     const std::string& prompt, int timeout) -> ProviderResponse;
};

} // namespace euxis::cli
