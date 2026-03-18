#pragma once

#include "euxis/core/router.hpp"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::cli {

/// Cost tier levels (ascending cost).
enum class Tier { Routine, Data, Code, Reason };

/// Convert tier enum to string label.
auto tier_label(Tier t) -> std::string;

/// Parse a tier string ("routine", "data", "code", "reason").
auto parse_tier(const std::string& s) -> Tier;

/// Provider/model selection result.
struct ModelSelection {
    std::string provider;
    std::string model;
    Tier tier;
    double estimated_cost_per_1m{0.0};
    std::string route_reason;   // Explainable routing decision
    std::string task_class;     // Semantic class that triggered this route
};

/// Options for strategy-based routing.
struct RouteOptions {
    std::string provider_override;   // --provider CLI flag
    bool local_only{false};          // --local-only / EUXIS_LOCAL_ONLY
    bool verbose{false};             // emit route reasoning
};

/// Semantic task class for strategy routing.
/// Maps to provider_strategy.json "defaults" keys.
struct TaskClassRoute {
    std::string primary;
    std::vector<std::string> fallback;
};

/// Provider detection + tier-based + strategy-based model routing.
class ProviderRouter {
public:
    explicit ProviderRouter(const std::string& data_dir);

    /// Detect the active provider from environment.
    [[nodiscard]] auto detect_provider() const -> std::string;

    /// Select the optimal model for the given tier.
    [[nodiscard]] auto select_model(Tier tier) const -> ModelSelection;

    /// Analyze a task description and infer its tier.
    [[nodiscard]] auto analyze_task_tier(const std::string& task) const -> Tier;

    /// Get the model for a specific agent + task combination (tier-based).
    [[nodiscard]] auto route(const std::string& agent_tier,
                             const std::string& prompt,
                             const std::string& priority = "") const -> ModelSelection;

    /// Route with flash-mode overrides.
    [[nodiscard]] auto route_flash(const std::string& agent_id,
                                   const std::string& agent_tier,
                                   const std::string& prompt) const -> ModelSelection;

    /// Route with standard-mode overrides.
    [[nodiscard]] auto route_standard(const std::string& agent_id,
                                      const std::string& agent_tier,
                                      const std::string& prompt) const -> ModelSelection;

    /// Route with forensic-mode overrides (highest-capability models).
    [[nodiscard]] auto route_forensic(const std::string& agent_id,
                                      const std::string& agent_tier,
                                      const std::string& prompt) const -> ModelSelection;

    // --- Strategy routing (new) ---

    /// Classify a task into a semantic class based on agent, pillar, and prompt.
    [[nodiscard]] auto classify_task_class(const std::string& task,
                                           const std::string& agent_id = {},
                                           const std::string& pillar = {}) const -> std::string;

    /// Route using the provider strategy config.
    /// Resolution order: explicit override > strategy > mode override > tier fallback.
    [[nodiscard]] auto route_by_strategy(const std::string& task_class,
                                          const std::string& agent_id,
                                          const std::string& agent_tier,
                                          const std::string& prompt,
                                          const RouteOptions& opts = {}) const -> ModelSelection;

    /// Full routing pipeline: classify + route with overrides and fallbacks.
    [[nodiscard]] auto route_with_policy(const std::string& task,
                                          const std::string& agent_id,
                                          const std::string& agent_tier,
                                          const std::string& pillar = {},
                                          const RouteOptions& opts = {}) const -> ModelSelection;

    // --- Availability ---

    /// Check if local inference (Ollama) is available.
    [[nodiscard]] auto local_available() const -> bool;

    /// Check if a specific tool/provider is available on the system.
    [[nodiscard]] auto tool_available(const std::string& tool) const -> bool;

    /// List all available providers (including tools like aider, kiro).
    [[nodiscard]] auto available_providers() const -> std::vector<std::string>;

    /// Print router status to stdout (includes strategy info).
    void print_status() const;

    /// Get fallback model chain for a given model.
    [[nodiscard]] auto model_fallback_chain(const std::string& model) const -> std::vector<ModelSelection>;

    /// Check if strategy config is loaded.
    [[nodiscard]] auto strategy_loaded() const -> bool { return strategy_loaded_; }

private:
    std::string data_dir_;
    nlohmann::json config_;
    nlohmann::json strategy_config_;
    bool strategy_loaded_{false};

    struct TierModels {
        std::string routine;
        std::string data;
        std::string code;
        std::string reason;
    };
    TierModels models_;

    std::unique_ptr<euxis::core::FinOpsRouter> router_;

    /// Per-agent model overrides (loaded from router.json).
    struct ModeOverride {
        std::string provider;
        std::string model;
        double cost{0.5};
    };
    std::unordered_map<std::string, ModeOverride> flash_overrides_;
    std::unordered_map<std::string, ModeOverride> standard_overrides_;
    std::unordered_map<std::string, ModeOverride> forensic_overrides_;

    /// Strategy routing data (loaded from provider_strategy.json).
    std::unordered_map<std::string, TaskClassRoute> strategy_defaults_;
    std::unordered_map<std::string, std::string> agent_class_hints_;
    std::unordered_map<std::string, std::vector<std::string>> classification_keywords_;

    void load_config();
    void load_strategy_config();
    void load_env_overrides();

    /// Look up the model for a provider + task class from strategy config.
    auto strategy_model(const std::string& provider,
                        const std::string& task_class) const -> std::string;

    /// Check env-var overrides for a given task class.
    auto env_provider_for_class(const std::string& task_class) const -> std::string;
};

} // namespace euxis::cli
