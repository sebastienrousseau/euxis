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
    load_strategy_config();
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
                ModeOverride fo;
                fo.provider = ovr.value("provider", "gemini");
                fo.model = ovr.value("model", "gemini-2.5-flash");
                fo.cost = ovr.value("cost", 0.5);
                flash_overrides_[agent_id] = fo;
            }
        }
        if (config_.contains("standard_overrides")) {
            for (auto& [agent_id, ovr] : config_["standard_overrides"].items()) {
                ModeOverride so;
                so.provider = ovr.value("provider", "claude");
                so.model = ovr.value("model", "claude-sonnet-4-6");
                so.cost = ovr.value("cost", 3.0);
                standard_overrides_[agent_id] = so;
            }
        }
        if (config_.contains("forensic_overrides")) {
            for (auto& [agent_id, ovr] : config_["forensic_overrides"].items()) {
                ModeOverride fo;
                fo.provider = ovr.value("provider", "claude");
                fo.model = ovr.value("model", "claude-opus-4-6");
                fo.cost = ovr.value("cost", 15.0);
                forensic_overrides_[agent_id] = fo;
            }
        }
    } catch (const std::exception& e) { spdlog::warn("router.json parse error: {}", e.what()); }
}

void ProviderRouter::load_strategy_config() {
    auto path = std::filesystem::path(data_dir_) / "config" / "provider_strategy.json";
    if (!std::filesystem::exists(path)) return;
    try {
        std::ifstream f(path);
        strategy_config_ = nlohmann::json::parse(f);
        strategy_loaded_ = true;

        // Load defaults (task class → primary + fallback)
        if (strategy_config_.contains("defaults")) {
            for (auto& [tc, route] : strategy_config_["defaults"].items()) {
                TaskClassRoute tcr;
                tcr.primary = route.value("primary", "claude");
                if (route.contains("fallback")) {
                    for (const auto& fb : route["fallback"]) {
                        tcr.fallback.push_back(fb.get<std::string>());
                    }
                }
                strategy_defaults_[tc] = std::move(tcr);
            }
        }

        // Load agent → task class hints
        if (strategy_config_.contains("agent_class_hints")) {
            for (auto& [agent, cls] : strategy_config_["agent_class_hints"].items()) {
                agent_class_hints_[agent] = cls.get<std::string>();
            }
        }

        // Load classification keywords
        if (strategy_config_.contains("classification_keywords")) {
            for (auto& [cls, kws] : strategy_config_["classification_keywords"].items()) {
                std::vector<std::string> keywords;
                for (const auto& kw : kws) {
                    keywords.push_back(kw.get<std::string>());
                }
                classification_keywords_[cls] = std::move(keywords);
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("provider_strategy.json parse error: {}", e.what());
        strategy_loaded_ = false;
    }
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
        return {detect_provider(), ovr, tier, 0.0, "explicit model override", ""};
    }
    // Check for local-only mode
    if (const char* lo = std::getenv("EUXIS_LOCAL_ONLY")) {
        std::string local(lo);
        if (local == "true" || local == "1") {
            std::string local_model = "qwen2.5-coder:7b";
            if (const char* lm = std::getenv("EUXIS_LOCAL_MODEL")) local_model = lm;
            return {"ollama", local_model, tier, 0.0, "local-only mode", "private_coding"};
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
    return {provider, model, tier, cost, "tier-based: " + tier_label(tier), ""};
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
            case Tier::Routine: sel.model = "claude-haiku-4-5"; break;
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

    sel.route_reason = "tier-based: " + tier_label(effective);
    return sel;
}

auto ProviderRouter::route_flash(const std::string& agent_id,
                                  const std::string& agent_tier,
                                  const std::string& prompt) const -> ModelSelection {
    auto it = flash_overrides_.find(agent_id);
    if (it != flash_overrides_.end()) {
        const auto& ovr = it->second;
        return {ovr.provider, ovr.model, Tier::Code, ovr.cost,
                "flash override for " + agent_id, ""};
    }
    return route(agent_tier, prompt, "swarm");
}

auto ProviderRouter::route_standard(const std::string& agent_id,
                                     const std::string& agent_tier,
                                     const std::string& prompt) const -> ModelSelection {
    auto it = standard_overrides_.find(agent_id);
    if (it != standard_overrides_.end()) {
        const auto& ovr = it->second;
        return {ovr.provider, ovr.model, Tier::Code, ovr.cost,
                "standard override for " + agent_id, ""};
    }
    return route(agent_tier, prompt, "swarm");
}

auto ProviderRouter::route_forensic(const std::string& agent_id,
                                     const std::string& agent_tier,
                                     const std::string& prompt) const -> ModelSelection {
    auto it = forensic_overrides_.find(agent_id);
    if (it != forensic_overrides_.end()) {
        const auto& ovr = it->second;
        return {ovr.provider, ovr.model, Tier::Reason, ovr.cost,
                "forensic override for " + agent_id, ""};
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

// =====================================================================
//  STRATEGY ROUTING
// =====================================================================

/// Check if an API provider has credentials/connectivity available.
static auto api_provider_usable(const std::string& provider) -> bool {
    if (provider == "claude" || provider == "anthropic") return std::getenv("ANTHROPIC_API_KEY") != nullptr;
    if (provider == "openai" || provider == "codex") return std::getenv("OPENAI_API_KEY") != nullptr;
    if (provider == "gemini") return (std::getenv("GEMINI_API_KEY") != nullptr || std::getenv("GOOGLE_API_KEY") != nullptr);
    if (provider == "ollama") return Process::available("ollama");
    return false;
}

auto ProviderRouter::classify_task_class(const std::string& task,
                                          const std::string& agent_id,
                                          const std::string& pillar) const -> std::string {
    // 1. Check explicit local-only env
    if (const char* lo = std::getenv("EUXIS_LOCAL_ONLY")) {
        std::string local(lo);
        if (local == "true" || local == "1") return "private_coding";
    }

    // 2. Check agent_id hints from strategy config
    if (!agent_id.empty()) {
        auto it = agent_class_hints_.find(agent_id);
        if (it != agent_class_hints_.end()) return it->second;
    }

    // 3. Check pillar-based classification
    if (!pillar.empty()) {
        if (pillar == "security" || pillar == "supply-chain") return "security";
        if (pillar == "docs" || pillar == "documentation") return "research";
        if (pillar == "architecture" || pillar == "design") return "architecture";
        if (pillar == "testing" || pillar == "execution") return "coding";
        if (pillar == "review" || pillar == "audit") return "audit";
    }

    // 4. Keyword classification from task prompt
    std::string lower = task;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Check specific classes first (more specific → less specific)
    // Order matters: check narrow classes before broad ones
    static const std::vector<std::string> class_priority = {
        "surgical_edit", "terminal_automation", "private_coding",
        "multilingual_analysis", "policy_synthesis",
        "security", "deep_research", "architecture", "audit",
        "research", "coding"
    };

    for (const auto& cls : class_priority) {
        auto it = classification_keywords_.find(cls);
        if (it != classification_keywords_.end()) {
            for (const auto& kw : it->second) {
                if (lower.find(kw) != std::string::npos) return cls;
            }
        }
    }

    // 5. Default: use tier-based heuristic to pick a class
    auto tier = analyze_task_tier(task);
    switch (tier) {
        case Tier::Reason:  return "architecture";
        case Tier::Code:    return "coding";
        case Tier::Data:    return "research";
        case Tier::Routine: return "coding";
    }
    return "coding";
}

auto ProviderRouter::strategy_model(const std::string& provider,
                                     const std::string& task_class) const -> std::string {
    if (!strategy_config_.contains("models")) return "";
    auto& models = strategy_config_["models"];
    if (!models.contains(provider)) return "";
    auto& pm = models[provider];
    if (pm.contains(task_class)) return pm[task_class].get<std::string>();
    if (pm.contains("default")) return pm["default"].get<std::string>();
    return "";
}

auto ProviderRouter::env_provider_for_class(const std::string& task_class) const -> std::string {
    // Map task classes to env var names
    // EUXIS_DEFAULT_RESEARCH_PROVIDER, EUXIS_DEFAULT_CODING_PROVIDER, etc.
    static const std::unordered_map<std::string, std::string> class_env_map = {
        {"research",              "EUXIS_DEFAULT_RESEARCH_PROVIDER"},
        {"multilingual_analysis", "EUXIS_DEFAULT_RESEARCH_PROVIDER"},
        {"policy_synthesis",      "EUXIS_DEFAULT_RESEARCH_PROVIDER"},
        {"coding",                "EUXIS_DEFAULT_CODING_PROVIDER"},
        {"architecture",          "EUXIS_DEFAULT_CODING_PROVIDER"},
        {"audit",                 "EUXIS_DEFAULT_CODING_PROVIDER"},
        {"deep_research",         "EUXIS_DEFAULT_RESEARCH_PROVIDER"},
        {"security",              "EUXIS_DEFAULT_SECURITY_PROVIDER"},
        {"private_coding",        "EUXIS_DEFAULT_CODING_PROVIDER"},
        {"surgical_edit",         "EUXIS_DEFAULT_CODING_PROVIDER"},
        {"terminal_automation",   "EUXIS_DEFAULT_CODING_PROVIDER"},
    };

    auto it = class_env_map.find(task_class);
    if (it != class_env_map.end()) {
        if (const char* val = std::getenv(it->second.c_str())) return val;
    }
    return "";
}

auto ProviderRouter::route_by_strategy(const std::string& task_class,
                                        const std::string& /*agent_id*/,
                                        const std::string& agent_tier,
                                        const std::string& prompt,
                                        const RouteOptions& opts) const -> ModelSelection {
    // 1. Explicit provider override (--provider flag)
    if (!opts.provider_override.empty()) {
        std::string model = strategy_model(opts.provider_override, task_class);
        if (model.empty()) model = opts.provider_override + "-default";
        auto tier = analyze_task_tier(prompt);
        return {opts.provider_override, model, std::max(parse_tier(agent_tier), tier), 0.0,
                "explicit --provider override: " + opts.provider_override, task_class};
    }

    // 2. Local-only enforcement
    if (opts.local_only) {
        std::string model = strategy_model("ollama", task_class);
        if (model.empty()) model = "llama4:maverick";
        if (const char* lm = std::getenv("EUXIS_LOCAL_MODEL")) model = lm;
        return {"ollama", model, parse_tier(agent_tier), 0.0,
                "local-only mode; all routes forced to Ollama", "private_coding"};
    }

    // 3. Env-var override for this task class
    auto env_provider = env_provider_for_class(task_class);
    if (!env_provider.empty()) {
        std::string model = strategy_model(env_provider, task_class);
        if (model.empty()) model = env_provider + "-default";
        return {env_provider, model, parse_tier(agent_tier), 0.0,
                "env override for " + task_class + ": " + env_provider, task_class};
    }

    // 4. Strategy config lookup
    auto it = strategy_defaults_.find(task_class);
    if (it != strategy_defaults_.end()) {
        const auto& route = it->second;

        // Try primary provider
        std::string primary = route.primary;
        bool primary_is_tool = (primary == "aider" || primary == "kiro" || primary == "shellgpt");

        if (primary_is_tool) {
            // For tools, check availability
            if (tool_available(primary)) {
                std::string model = strategy_model(primary, task_class);
                if (model.empty()) model = primary;
                return {primary, model, parse_tier(agent_tier), 0.0,
                        task_class + " task; " + primary + " preferred (tool available)",
                        task_class};
            }
        } else if (api_provider_usable(primary)) {
            // For API providers, use directly if credentials available
            std::string model = strategy_model(primary, task_class);
            if (model.empty()) {
                // Fall back to tier-based model for this provider
                auto tier = std::max(parse_tier(agent_tier), analyze_task_tier(prompt));
                if (primary == "claude") {
                    switch (tier) {
                        case Tier::Routine: model = "claude-haiku-4-5"; break;
                        case Tier::Data:
                        case Tier::Code:    model = models_.code; break;
                        case Tier::Reason:  model = models_.reason; break;
                    }
                } else if (primary == "openai") {
                    model = "gpt-5.4";
                } else if (primary == "gemini") {
                    model = "gemini-2.5-pro";
                } else if (primary == "ollama") {
                    model = "llama4:maverick";
                } else {
                    model = primary + "-default";
                }
            }
            return {primary, model, std::max(parse_tier(agent_tier), analyze_task_tier(prompt)), 0.0,
                    task_class + " task; " + primary + " preferred for " + task_class,
                    task_class};
        }

        // Try fallbacks
        for (const auto& fb : route.fallback) {
            bool is_tool = (fb == "aider" || fb == "kiro" || fb == "shellgpt");
            if (is_tool && !tool_available(fb)) continue;
            if (!is_tool && !api_provider_usable(fb)) continue;

            std::string model = strategy_model(fb, task_class);
            if (model.empty()) {
                if (fb == "claude") model = models_.code;
                else if (fb == "openai") model = "gpt-5.4";
                else if (fb == "gemini") model = "gemini-2.5-pro";
                else if (fb == "ollama") model = "llama4:maverick";
                else model = fb;
            }
            return {fb, model, std::max(parse_tier(agent_tier), analyze_task_tier(prompt)), 0.0,
                    task_class + " task; " + fb + " (fallback, " + primary + " unavailable)",
                    task_class};
        }
    }

    // 5. No strategy match — fall back to tier-based routing
    auto sel = route(agent_tier, prompt);
    sel.route_reason = "tier fallback (no strategy match for class '" + task_class + "')";
    sel.task_class = task_class;
    return sel;
}

auto ProviderRouter::route_with_policy(const std::string& task,
                                        const std::string& agent_id,
                                        const std::string& agent_tier,
                                        const std::string& pillar,
                                        const RouteOptions& opts) const -> ModelSelection {
    // Classify the task
    auto task_class = classify_task_class(task, agent_id, pillar);

    // Route using strategy
    auto result = route_by_strategy(task_class, agent_id, agent_tier, task, opts);

    // Verbose output
    if (opts.verbose) {
        std::cerr << "Routing: " << (agent_id.empty() ? "unknown" : agent_id)
                  << " -> " << result.provider << " / " << result.model << "\n"
                  << "Reason: " << result.route_reason << "\n";
    }

    return result;
}

// --- Availability ---

auto ProviderRouter::local_available() const -> bool { return Process::available("ollama"); }

auto ProviderRouter::tool_available(const std::string& tool) const -> bool {
    return Process::available(tool);
}

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
    std::cout << "  " << tr("Available providers:") << "\n";
    for (const auto& p : avail) std::cout << "    - " << p << "\n";
    if (avail.empty()) std::cout << "    (" << tr("none detected") << ")\n";

    if (strategy_loaded_) {
        std::cout << "\n  " << tr("Provider strategy:") << "\n";
        // Show the routing policy summary
        static const std::vector<std::pair<std::string, std::string>> policy_summary = {
            {"research",              "OpenAI GPT-5.4"},
            {"coding",                "Claude Sonnet / Opus 4.6"},
            {"architecture",          "Claude Opus 4.6"},
            {"audit",                 "Claude Opus 4.6"},
            {"deep_research",         "Gemini 2.5 Pro"},
            {"security",              "Gemini 2.5 Pro"},
            {"private_coding",        "Ollama (local)"},
            {"surgical_edit",         "Aider"},
            {"terminal_automation",   "Kiro / ShellGPT"},
        };
        for (const auto& [cls, desc] : policy_summary) {
            auto it = strategy_defaults_.find(cls);
            std::string primary = it != strategy_defaults_.end() ? it->second.primary : "?";
            std::cout << "    " << cls;
            // Pad
            for (size_t i = cls.size(); i < 24; ++i) std::cout << ' ';
            std::cout << "-> " << desc << "\n";
        }
    }
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
                0.0, "", ""
            });
        }
        return chain;
    }

    // Default fallback chains by provider
    if (model.find("claude") != std::string::npos || model.find("anthropic") != std::string::npos) {
        return {
            {"openai", "gpt-5.4", Tier::Code, 5.0, "", ""},
            {"gemini", "gemini-2.5-flash", Tier::Code, 0.5, "", ""},
            {"ollama", "qwen2.5-coder:7b", Tier::Code, 0.0, "", ""}
        };
    }
    if (model.find("gpt") != std::string::npos || model.find("openai") != std::string::npos) {
        return {
            {"claude", "claude-sonnet-4-6", Tier::Code, 3.0, "", ""},
            {"gemini", "gemini-2.5-flash", Tier::Code, 0.5, "", ""},
            {"ollama", "qwen2.5-coder:7b", Tier::Code, 0.0, "", ""}
        };
    }
    if (model.find("gemini") != std::string::npos) {
        return {
            {"claude", "claude-sonnet-4-6", Tier::Code, 3.0, "", ""},
            {"openai", "gpt-5.4", Tier::Code, 5.0, "", ""},
            {"ollama", "qwen2.5-coder:7b", Tier::Code, 0.0, "", ""}
        };
    }
    // Generic: try all major providers
    return {
        {"claude", "claude-sonnet-4-6", Tier::Code, 3.0, "", ""},
        {"openai", "gpt-5.4", Tier::Code, 5.0, "", ""},
        {"gemini", "gemini-2.5-flash", Tier::Code, 0.5, "", ""},
        {"ollama", "qwen2.5-coder:7b", Tier::Code, 0.0, "", ""}
    };
}

} // namespace euxis::cli
