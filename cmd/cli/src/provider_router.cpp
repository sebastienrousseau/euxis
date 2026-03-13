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
    models_.routine = "gemini-2.0-flash-lite";
    models_.data    = "deepseek-v3";
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
    } catch (...) {}
}

void ProviderRouter::load_env_overrides() {}

auto ProviderRouter::detect_provider() const -> std::string { return "claude"; }
auto ProviderRouter::select_model(Tier tier) const -> ModelSelection { return {"claude", models_.reason, tier, 0.0}; }

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

    if (provider == "claude") sel.model = models_.reason;
    else if (provider == "gemini") sel.model = models_.routine;
    else if (provider == "ollama") sel.model = "qwen2.5-coder:7b";
    else sel.model = provider + "-default"; // opencode, aider, etc.

    return sel;
}

auto ProviderRouter::analyze_task_tier(const std::string& task) const -> Tier {
    static const std::vector<std::string> reason_keywords = {"architect", "design", "plan", "strategy", "complex", "critical", "security", "audit", "review"};
    static const std::vector<std::string> code_keywords = {"code", "implement", "refactor", "debug", "test", "build", "fix", "optimize", "develop"};
    std::string lower = task;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& kw : reason_keywords) if (lower.find(kw) != std::string::npos) return Tier::Reason;
    for (const auto& kw : code_keywords) if (lower.find(kw) != std::string::npos) return Tier::Code;
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

auto ProviderRouter::model_fallback_chain([[maybe_unused]] const std::string& model) const -> std::vector<ModelSelection> {
    return {{"ollama", "qwen2.5-coder:7b", Tier::Code, 0.0}};
}

} // namespace euxis::cli
