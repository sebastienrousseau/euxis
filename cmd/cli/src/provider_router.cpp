#include "euxis/cli/provider_router.hpp"
#include "euxis/cli/i18n.hpp"
#include "euxis/cli/provider_executor.hpp"
#include "euxis/cli/process.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <spdlog/spdlog.h>

namespace euxis::cli {

using euxis::cli::i18n::tr;

auto tier_label(Tier t) -> std::string {
    switch (t) {
        case Tier::Routine: return "routine";
        case Tier::Data:    return "data";
        case Tier::Code:    return "code";
        case Tier::Reason:  return "reason";
    }
    return "code";
}

auto parse_tier(const std::string& s) -> Tier {
    if (s == "routine") return Tier::Routine;
    if (s == "data")    return Tier::Data;
    if (s == "reason" || s == "core")  return Tier::Reason;
    return Tier::Code;
}

// --- ProviderRouter ---

ProviderRouter::ProviderRouter(const std::string& data_dir)
    : data_dir_(data_dir), router_(std::make_unique<euxis::core::FinOpsRouter>(100.0)) {
    // Defaults
    models_.routine = "gemini-2.5-flash-lite";
    models_.data    = "gemini-2.5-flash";
    models_.code    = "claude-sonnet-4-6";
    models_.reason  = "claude-opus-4-6";

    load_config();
}

void ProviderRouter::load_config() {
    auto config_path = std::filesystem::path(data_dir_) / "config" / "router.json";
    if (!std::filesystem::exists(config_path)) return;
    try {
        std::ifstream f(config_path);
        config_ = nlohmann::json::parse(f);
        if (config_.contains("models")) {
            auto& m = config_["models"];
            if (m.contains("routine")) models_.routine = m["routine"].get<std::string>();
            if (m.contains("data"))    models_.data = m["data"].get<std::string>();
            if (m.contains("code"))    models_.code = m["code"].get<std::string>();
            if (m.contains("reason"))  models_.reason = m["reason"].get<std::string>();
        }
        if (config_.contains("flash_overrides")) {
            for (auto& [agent_id, ovr] : config_["flash_overrides"].items()) {
                FlashOverride fo;
                fo.provider = ovr.value("provider", "gemini");
                fo.model = ovr.value("model", "gemini-2.5-flash");
                fo.cost = ovr.value("cost", 0.5);
                flash_overrides_[agent_id] = fo;
            }
        }
    } catch (...) {}
}

void ProviderRouter::load_env_overrides() {}

auto ProviderRouter::detect_provider() const -> std::string {
    // Priority: explicit override > env vars > default
    if (const char* dp = std::getenv("EUXIS_DEFAULT_PROVIDER")) return dp;
    if (std::getenv("ANTHROPIC_API_KEY")) return "claude";
    if (std::getenv("OPENAI_API_KEY")) return "openai";
    if (std::getenv("GEMINI_API_KEY") || std::getenv("GOOGLE_API_KEY")) return "gemini";
    return "claude";
}

auto ProviderRouter::select_model(Tier tier) const -> ModelSelection {
    // Check for model override
    if (const char* ovr = std::getenv("EUXIS_MODEL_OVERRIDE")) {
        return {detect_provider(), ovr, tier, 0.0};
    }
    // Check for local-only mode
    if (const char* lo = std::getenv("EUXIS_LOCAL_ONLY")) {
        std::string local(lo);
        if (local == "true" || local == "1") {
            std::string local_model = "qwen2.5-coder:7b";
            if (const char* lm = std::getenv("EUXIS_LOCAL_MODEL")) local_model = lm;
            return {"ollama", local_model, tier, 0.0};
        }
    }
    std::string provider = detect_provider();
    std::string model;
    double cost = 0.0;
    switch (tier) {
        case Tier::Routine: model = models_.routine; cost = 0.10; break;
        case Tier::Data:    model = models_.data;    cost = 0.25; break;
        case Tier::Code:    model = models_.code;    cost = 3.00; break;
        case Tier::Reason:  model = models_.reason;  cost = 15.0; break;
    }
    return {provider, model, tier, cost};
}

auto ProviderRouter::route(const std::string& agent_tier,
                            const std::string& task,
                            const std::string& priority) const -> ModelSelection {
    Tier at = parse_tier(agent_tier);
    Tier tt = analyze_task_tier(task);
    Tier effective = std::max(at, tt);

    ModelSelection sel;
    sel.tier = effective;
    
    // Use the core FinOpsRouter to pick the provider
    std::string provider = router_->select_provider(tier_label(effective), priority);
    sel.provider = provider;

    // Map provider + tier to the appropriate model
    if (provider == "claude") {
        switch (effective) {
            case Tier::Routine: sel.model = "claude-haiku-3-5"; break;
            case Tier::Data:    sel.model = models_.code; break;   // sonnet
            case Tier::Code:    sel.model = models_.code; break;   // sonnet
            case Tier::Reason:  sel.model = models_.reason; break; // opus
        }
    } else if (provider == "gemini") {
        switch (effective) {
            case Tier::Routine: sel.model = models_.routine; break; // flash-lite
            case Tier::Data:    sel.model = "gemini-2.5-flash"; break;
            case Tier::Code:    sel.model = "gemini-2.5-flash"; break;
            case Tier::Reason:  sel.model = "gemini-2.5-pro"; break;
        }
    } else if (provider == "ollama") {
        sel.model = "qwen2.5-coder:7b";
    } else {
        sel.model = provider + "-default";
    }

    return sel;
}

auto ProviderRouter::route_flash(const std::string& agent_id,
                                  const std::string& agent_tier,
                                  const std::string& prompt) const -> ModelSelection {
    auto it = flash_overrides_.find(agent_id);
    if (it != flash_overrides_.end()) {
        const auto& ovr = it->second;
        return {ovr.provider, ovr.model, Tier::Code, ovr.cost};
    }
    return route(agent_tier, prompt, "swarm");
}

auto ProviderRouter::analyze_task_tier(const std::string& task) const -> Tier {
    static const std::vector<std::string> reason_keywords = {"architect", "design", "plan", "strategy", "complex", "critical", "security", "audit", "review"};
    static const std::vector<std::string> code_keywords = {"code", "implement", "refactor", "debug", "test", "build", "fix", "optimize", "develop"};
    static const std::vector<std::string> data_keywords = {"data", "analyze", "summarize", "report", "aggregate", "query", "extract", "parse", "transform"};
    std::string lower = task;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& kw : reason_keywords) if (lower.find(kw) != std::string::npos) return Tier::Reason;
    for (const auto& kw : code_keywords) if (lower.find(kw) != std::string::npos) return Tier::Code;
    for (const auto& kw : data_keywords) if (lower.find(kw) != std::string::npos) return Tier::Data;
    return Tier::Routine;
}

auto ProviderRouter::local_available() const -> bool { return Process::available("ollama"); }

auto ProviderRouter::available_providers() const -> std::vector<std::string> {
    std::vector<std::string> result;
    for (const auto& p : {"claude", "gemini", "openai", "ollama", "opencode", "aider", "sgpt", "kiro"}) {
        if (Process::available(p)) result.push_back(p);
    }
    return result;
}

void ProviderRouter::print_status() const {
    std::cout << tr("Provider Router Status") << "\n";
    auto avail = available_providers();
    for (const auto& p : avail) std::cout << "  - " << p << "\n";
}

auto ProviderRouter::model_fallback_chain(const std::string& model) const -> std::vector<ModelSelection> {
    // Check config for custom fallback chains
    if (config_.contains("model_fallback") && config_["model_fallback"].contains(model)) {
        std::vector<ModelSelection> chain;
        for (const auto& entry : config_["model_fallback"][model]) {
            chain.push_back({
                entry.value("provider", ""),
                entry.value("model", ""),
                Tier::Code,
                0.0
            });
        }
        return chain;
    }

    // Default fallback chains by provider
    if (model.find("claude") != std::string::npos || model.find("anthropic") != std::string::npos) {
        return {
            {"openai", "gpt-4o", Tier::Code, 5.0},
            {"gemini", "gemini-2.5-flash", Tier::Code, 0.5},
            {"ollama", "qwen2.5-coder:7b", Tier::Code, 0.0}
        };
    }
    if (model.find("gpt") != std::string::npos || model.find("openai") != std::string::npos) {
        return {
            {"claude", "claude-sonnet-4-6", Tier::Code, 3.0},
            {"gemini", "gemini-2.5-flash", Tier::Code, 0.5},
            {"ollama", "qwen2.5-coder:7b", Tier::Code, 0.0}
        };
    }
    if (model.find("gemini") != std::string::npos) {
        return {
            {"claude", "claude-sonnet-4-6", Tier::Code, 3.0},
            {"openai", "gpt-4o", Tier::Code, 5.0},
            {"ollama", "qwen2.5-coder:7b", Tier::Code, 0.0}
        };
    }
    // Generic: try all major providers
    return {
        {"claude", "claude-sonnet-4-6", Tier::Code, 3.0},
        {"openai", "gpt-4o", Tier::Code, 5.0},
        {"gemini", "gemini-2.5-flash", Tier::Code, 0.5},
        {"ollama", "qwen2.5-coder:7b", Tier::Code, 0.0}
    };
}

} // namespace euxis::cli
