#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "errors.hpp"

namespace euxis::runtime {

/** @brief Specific model configuration for a provider. */
struct ModelSpec {
    std::string provider;
    std::string model;
    std::string tier;
    double estimated_cost_per_1m{0.0};
    nlohmann::json parameters = nlohmann::json::object();
};

/**
 * @brief Response from an LLM provider.
 */
struct ProviderResponse {
    bool success{false};
    std::string output;
    std::string error;
    int input_tokens{0};
    int output_tokens{0};
    double duration_ms{0.0};
};

/**
 * @brief Abstract interface for LLM backends (Ollama, Anthropic, etc.).
 */
class IProvider {
public:
    virtual ~IProvider() = default;

    /** @brief Map a tier and intent to a concrete model. */
    virtual auto route(const std::string& tier, const std::string& intent) -> ModelSpec = 0;

    /** @brief Change the active model for this provider. */
    virtual auto switch_model(const ModelSpec& spec) -> bool = 0;

    /**
     * @brief Execute a prompt against the model.
     */
    virtual auto execute(const std::string& model,
                         const std::string& prompt,
                         int timeout_ms = 30000,
                         const std::optional<std::vector<std::string>>& stop_sequences = std::nullopt,
                         std::function<void(const std::string&)> stream_callback = nullptr)
        -> ProviderResponse = 0;

    /** @brief Execute using a structured ModelSpec. */
    virtual auto execute(const ModelSpec& spec,
                         const std::string& prompt,
                         int timeout_ms = 30000,
                         const std::optional<std::vector<std::string>>& stop_sequences = std::nullopt,
                         std::function<void(const std::string&)> stream_callback = nullptr)
        -> ProviderResponse = 0;
};

} // namespace euxis::runtime
