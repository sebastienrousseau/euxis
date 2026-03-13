#pragma once

#include "euxis/cli/auth_profile_store.hpp"
#include "euxis/cli/provider_router.hpp"

#include <optional>
#include <string>

#include <functional>

namespace euxis::cli {

/// Result of a provider invocation.
struct ProviderResponse {
    bool success{false};
    std::string output;
    std::string error;
    int exit_code{-1};
    double duration_ms{0.0};
    std::optional<CooldownReason> failure_reason;
};

/// Invokes LLM providers via their CLI or API.
class ProviderExecutor {
public:
    explicit ProviderExecutor(const std::string& data_dir);

    /// Send a prompt to the provider specified by the model selection.
    /// When auth is provided, uses that token; otherwise falls back to env vars.
    [[nodiscard]] auto execute(const ModelSelection& selection,
                                const std::string& prompt,
                                int timeout_seconds = 120,
                                std::optional<ResolvedAuth> auth = std::nullopt,
                                std::function<void(const std::string&)> on_chunk = nullptr) -> ProviderResponse;

    /// Load an agent's system prompt from its prompt file (strips YAML frontmatter).
    [[nodiscard]] static auto load_agent_prompt(const std::string& euxis_home,
                                                 const std::string& prompt_path) -> std::string;

    /// Build a combined prompt from system prompt, task, and optional context.
    [[nodiscard]] static auto build_prompt(const std::string& system_prompt,
                                            const std::string& task,
                                            const std::string& context = {}) -> std::string;

    /// Resolve an Anthropic auth token from OAuth credentials or env var.
    /// Returns empty string if none found.
    [[nodiscard]] static auto resolve_anthropic_token() -> std::string;

    /// Classify an API error response into a cooldown reason.
    [[nodiscard]] static auto classify_error(int http_status,
                                              const std::string& body) -> std::optional<CooldownReason>;

    [[nodiscard]] auto auth_store() -> AuthProfileStore& { return auth_store_; }

private:
    std::string data_dir_;
    AuthProfileStore auth_store_;

    auto execute_claude(const std::string& model, const std::string& prompt,
                        int timeout, const std::optional<ResolvedAuth>& auth,
                        std::function<void(const std::string&)> on_chunk) -> ProviderResponse;
    auto execute_via_cli(const std::string& provider, const std::string& model,
                         const std::string& prompt, int timeout,
                         std::function<void(const std::string&)> on_chunk) -> ProviderResponse;
    auto execute_ollama(const std::string& model, const std::string& prompt, int timeout,
                        std::function<void(const std::string&)> on_chunk) -> ProviderResponse;
    auto execute_api(const std::string& provider, const std::string& model,
                     const std::string& prompt, int timeout,
                     const std::optional<ResolvedAuth>& auth,
                     std::function<void(const std::string&)> on_chunk) -> ProviderResponse;
};

} // namespace euxis::cli
