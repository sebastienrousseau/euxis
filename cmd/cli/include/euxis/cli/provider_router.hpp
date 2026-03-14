#pragma once

#include "euxis/core/router.hpp"
#include <memory>
#include <optional>
#include <string>
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
};

/// Provider detection + tier-based model routing.
class ProviderRouter {
public:
    explicit ProviderRouter(const std::string& data_dir);

    /// Detect the active provider from environment.
    [[nodiscard]] auto detect_provider() const -> std::string;

    /// Select the optimal model for the given tier.
    [[nodiscard]] auto select_model(Tier tier) const -> ModelSelection;

    /// Analyze a task description and infer its tier.
    [[nodiscard]] auto analyze_task_tier(const std::string& task) const -> Tier;

    /// Get the model for a specific agent + task combination.
    [[nodiscard]] auto route(const std::string& agent_tier,
                             const std::string& prompt,
                             const std::string& priority = "") const -> ModelSelection;

    /// Route with flash-mode overrides: checks per-agent flash config first,
    /// falls back to normal route() if no override exists.
    [[nodiscard]] auto route_flash(const std::string& agent_id,
                                   const std::string& agent_tier,
                                   const std::string& prompt) const -> ModelSelection;


    /// Check if local inference (Ollama) is available.
    [[nodiscard]] auto local_available() const -> bool;

    /// List all available providers.
    [[nodiscard]] auto available_providers() const -> std::vector<std::string>;

    /// Print router status to stdout.
    void print_status() const;

    /// Get fallback model chain for a given model.
    [[nodiscard]] auto model_fallback_chain(const std::string& model) const -> std::vector<ModelSelection>;

private:
    std::string data_dir_;
    nlohmann::json config_;

    struct TierModels {
        std::string routine;
        std::string data;
        std::string code;
        std::string reason;
    };
    TierModels models_;

    std::unique_ptr<euxis::core::FinOpsRouter> router_;

    /// Per-agent model overrides for flash mode (loaded from config).
    struct FlashOverride {
        std::string provider;
        std::string model;
        double cost{0.5};
    };
    std::unordered_map<std::string, FlashOverride> flash_overrides_;

    void load_config();
    void load_env_overrides();
};

} // namespace euxis::cli
