#include "euxis/cli/auth_profile_store.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <spdlog/spdlog.h>

namespace euxis::cli {

AuthProfileStore::AuthProfileStore(const std::string& data_dir)
    : data_dir_(data_dir)
{
    // Function-local static: lazy init avoids throwing during static-init phase
    // (bugprone-throwing-static-initialization).
    static const std::map<std::string, std::vector<std::string>> kDefaultFallbacks = {
        {"claude",    {"openai", "gemini", "ollama"}},
        {"anthropic", {"openai", "gemini", "ollama"}},
        {"openai",    {"claude", "gemini", "ollama"}},
        {"gemini",    {"claude", "openai", "ollama"}},
        {"ollama",    {"claude", "openai", "gemini"}},
    };

    auto config_dir = std::filesystem::path(data_dir) / "config";
    std::filesystem::create_directories(config_dir);
    store_path_ = (config_dir / "auth_profiles.json").string();

    // Load defaults
    fallback_chains_ = kDefaultFallbacks;

    reload();
}

// --- CRUD ---

void AuthProfileStore::add_api_key(const std::string& provider,
                                    const std::string& key,
                                    const std::string& label) {
    std::lock_guard lock(mutex_);

    std::string id = provider + ":" + (label.empty() ? "api_key" : label);

    // Check for duplicate
    auto it = std::find_if(profiles_.begin(), profiles_.end(),
        [&](const AuthProfile& p) { return p.id == id; });
    if (it != profiles_.end()) {
        it->api_key = key;
        it->label = label;
        return;
    }

    AuthProfile profile;
    profile.id = id;
    profile.provider = provider;
    profile.type = ProfileType::ApiKey;
    profile.label = label.empty() ? "API Key" : label;
    profile.api_key = key;
    profile.source = "manual";
    profiles_.push_back(std::move(profile));
}

void AuthProfileStore::add_oauth(const std::string& provider,
                                  const std::string& access_token,
                                  const std::string& refresh_token,
                                  int64_t expires_at,
                                  const std::string& email) {
    std::lock_guard lock(mutex_);

    std::string id = provider + ":" + (email.empty() ? "oauth" : email);

    auto it = std::find_if(profiles_.begin(), profiles_.end(),
        [&](const AuthProfile& p) { return p.id == id; });
    if (it != profiles_.end()) {
        it->access_token = access_token;
        it->refresh_token = refresh_token;
        it->expires_at = expires_at;
        it->email = email;
        return;
    }

    AuthProfile profile;
    profile.id = id;
    profile.provider = provider;
    profile.type = ProfileType::OAuth;
    profile.label = email.empty() ? "OAuth" : email;
    profile.access_token = access_token;
    profile.refresh_token = refresh_token;
    profile.expires_at = expires_at;
    profile.email = email;
    profile.source = "browser_oauth";
    profiles_.push_back(std::move(profile));
}

void AuthProfileStore::remove_profile(const std::string& id) {
    std::lock_guard lock(mutex_);
    std::erase_if(profiles_, [&](const AuthProfile& p) { return p.id == id; });
    cooldowns_.erase(id);
    if (session_pin_ == id) session_pin_.clear();
}

auto AuthProfileStore::profiles_for(const std::string& provider) const -> std::vector<AuthProfile> {
    std::lock_guard lock(mutex_);
    std::vector<AuthProfile> result;
    for (const auto& p : profiles_) {
        if (p.provider == provider) result.push_back(p);
    }
    return result;
}

auto AuthProfileStore::all_profiles() const -> std::vector<AuthProfile> {
    std::lock_guard lock(mutex_);
    return profiles_;
}

// --- Resolution ---

auto AuthProfileStore::resolve(const std::string& provider) -> std::optional<ResolvedAuth> {
    std::lock_guard lock(mutex_);
    return pick_best(provider);
}

auto AuthProfileStore::resolve_with_fallback(const std::string& provider)
    -> std::optional<ResolvedAuth> {
    std::lock_guard lock(mutex_);

    // Try primary provider first
    if (auto auth = pick_best(provider)) return auth;

    // Try fallback chain
    auto chain_it = fallback_chains_.find(provider);
    if (chain_it != fallback_chains_.end()) {
        for (const auto& fallback : chain_it->second) {
            if (auto auth = pick_best(fallback)) return auth;
        }
    }

    return std::nullopt;
}

// --- Session Pinning ---

void AuthProfileStore::pin_session(const std::string& profile_id) {
    std::lock_guard lock(mutex_);
    session_pin_ = profile_id;
}

void AuthProfileStore::clear_session_pin() {
    std::lock_guard lock(mutex_);
    session_pin_.clear();
}

// --- Cooldowns ---

void AuthProfileStore::report_failure(const std::string& profile_id,
                                       CooldownReason reason) {
    std::lock_guard lock(mutex_);
    auto& entry = cooldowns_[profile_id];
    entry.consecutive++;
    entry.reason = reason;
    entry.until_ms = now_ms() + compute_cooldown_ms(reason, entry.consecutive);

    spdlog::info("Auth profile {} cooled down for {}ms (reason={}, consecutive={})",
                 profile_id, compute_cooldown_ms(reason, entry.consecutive),
                 static_cast<int>(reason), entry.consecutive);
}

void AuthProfileStore::report_success(const std::string& profile_id) {
    std::lock_guard lock(mutex_);
    cooldowns_.erase(profile_id);

    // Update last_used_at
    for (auto& p : profiles_) {
        if (p.id == profile_id) {
            p.last_used_at = now_ms();
            break;
        }
    }
}

// --- Auto-Import ---

void AuthProfileStore::auto_import_claude_oauth() {
    const char* home = std::getenv("HOME");
    if (!home) return;

    auto creds_path = std::filesystem::path(home) / ".claude" / ".credentials.json";
    if (!std::filesystem::exists(creds_path)) return;

    try {
        std::ifstream f(creds_path);
        auto creds = nlohmann::json::parse(f);

        if (!creds.contains("claudeAiOauth")) return;
        auto& oauth = creds["claudeAiOauth"];

        std::string access_token;
        std::string refresh_token;
        int64_t expires_at = 0;

        if (oauth.contains("accessToken"))
            access_token = oauth["accessToken"].get<std::string>();
        if (oauth.contains("refreshToken"))
            refresh_token = oauth["refreshToken"].get<std::string>();
        if (oauth.contains("expiresAt"))
            expires_at = oauth["expiresAt"].get<int64_t>();

        if (access_token.empty()) return;

        std::lock_guard lock(mutex_);

        // Check if already imported
        for (const auto& p : profiles_) {
            if (p.source == "claude_code_import" && p.provider == "claude") return;
        }

        AuthProfile profile;
        profile.id = "claude:claude_code";
        profile.provider = "claude";
        profile.type = ProfileType::OAuth;
        profile.label = "Claude Code";
        profile.access_token = access_token;
        profile.refresh_token = refresh_token;
        profile.expires_at = expires_at;
        profile.source = "claude_code_import";
        profiles_.push_back(std::move(profile));

        spdlog::info("Auto-imported Anthropic OAuth from Claude Code");
    } catch (const std::exception& e) {
        spdlog::warn("Failed to import Claude credentials: {}", e.what());
    }
}

void AuthProfileStore::auto_import_gemini_oauth() {
    const char* home = std::getenv("HOME");
    if (!home) return;

    auto creds_path = std::filesystem::path(home) / ".gemini" / "oauth_creds.json";
    if (!std::filesystem::exists(creds_path)) return;

    try {
        std::ifstream f(creds_path);
        auto creds = nlohmann::json::parse(f);

        std::string access_token;
        std::string refresh_token;
        int64_t expires_at = 0;
        std::string email;

        if (creds.contains("access_token"))
            access_token = creds["access_token"].get<std::string>();
        if (creds.contains("refresh_token"))
            refresh_token = creds["refresh_token"].get<std::string>();

        // Handle both "expiry_date" (ms) and "expires_at" (s)
        if (creds.contains("expiry_date")) {
            auto val = creds["expiry_date"];
            if (val.is_number()) expires_at = val.get<int64_t>();
        } else if (creds.contains("expires_at")) {
            auto val = creds["expires_at"];
            if (val.is_number()) expires_at = static_cast<int64_t>(val.get<double>() * 1000);
        }

        if (creds.contains("email"))
            email = creds["email"].get<std::string>();

        if (access_token.empty()) return;

        std::lock_guard lock(mutex_);

        for (const auto& p : profiles_) {
            if (p.source == "gemini_import" && p.provider == "gemini") return;
        }

        AuthProfile profile;
        profile.id = "gemini:" + (email.empty() ? "google_oauth" : email);
        profile.provider = "gemini";
        profile.type = ProfileType::OAuth;
        profile.label = email.empty() ? "Gemini CLI" : email;
        profile.access_token = access_token;
        profile.refresh_token = refresh_token;
        profile.expires_at = expires_at;
        profile.email = email;
        profile.source = "gemini_import";
        profiles_.push_back(std::move(profile));

        spdlog::info("Auto-imported Gemini OAuth from Gemini CLI");
    } catch (const std::exception& e) {
        spdlog::warn("Failed to import Gemini credentials: {}", e.what());
    }
}

void AuthProfileStore::import_env_vars() {
    std::lock_guard lock(mutex_);

    auto import_key = [&](const char* env_name, const std::string& provider) {
        const char* val = std::getenv(env_name);
        if (!val || !val[0]) return;

        // Check if already have an env-sourced profile for this provider
        for (const auto& p : profiles_) {
            if (p.source == "env" && p.provider == provider) return;
        }

        AuthProfile profile;
        profile.id = provider + ":env";
        profile.provider = provider;
        profile.type = ProfileType::ApiKey;
        profile.label = std::string(env_name);
        profile.api_key = val;
        profile.source = "env";
        profiles_.push_back(std::move(profile));
    };

    import_key("ANTHROPIC_API_KEY", "claude");
    import_key("OPENAI_API_KEY", "openai");
    import_key("GEMINI_API_KEY", "gemini");
    import_key("GOOGLE_API_KEY", "gemini");
}

// --- Persistence ---

void AuthProfileStore::save() {
    std::lock_guard lock(mutex_);
    try {
        auto j = to_json();
        std::ofstream f(store_path_);
        f << j.dump(2);
        f.close();
        std::filesystem::permissions(store_path_,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to save auth profiles: {}", e.what());
    }
}

void AuthProfileStore::reload() {
    std::lock_guard lock(mutex_);
    if (!std::filesystem::exists(store_path_)) return;

    try {
        std::ifstream f(store_path_);
        auto j = nlohmann::json::parse(f);
        load_from_json(j);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load auth profiles: {}", e.what());
    }
}

// --- Fallback chain access ---

auto AuthProfileStore::fallback_chain(const std::string& provider) const
    -> std::vector<std::string> {
    std::lock_guard lock(mutex_);
    auto it = fallback_chains_.find(provider);
    if (it != fallback_chains_.end()) return it->second;
    return {};
}

void AuthProfileStore::set_fallback_chain(const std::string& provider,
                                           const std::vector<std::string>& chain) {
    std::lock_guard lock(mutex_);
    fallback_chains_[provider] = chain;
}

auto AuthProfileStore::cooldown_remaining_ms(const std::string& profile_id) const -> int64_t {
    std::lock_guard lock(mutex_);
    auto it = cooldowns_.find(profile_id);
    if (it == cooldowns_.end()) return 0;
    auto remaining = it->second.until_ms - now_ms();
    return remaining > 0 ? remaining : 0;
}

// --- Internal ---

auto AuthProfileStore::pick_best(const std::string& provider) const
    -> std::optional<ResolvedAuth> {
    // Gather eligible profiles
    std::vector<const AuthProfile*> candidates;
    bool has_cli_import = false;

    for (const auto& p : profiles_) {
        if (p.provider != provider) continue;
        if (is_cooled_down(p.id)) continue;

        // Skip expired OAuth tokens
        if (p.type == ProfileType::OAuth && p.expires_at > 0) {
            if (now_ms() >= p.expires_at) continue;
        }

        if (p.source == "claude_code_import" || p.source == "gemini_import") {
            has_cli_import = true;
        }

        candidates.push_back(&p);
    }

    if (candidates.empty()) return std::nullopt;

    // "CLI-first" policy: if we have a CLI-imported auth, ignore ephemeral 'env' keys
    // to avoid accidentally using a global API key when the user is signed in via CLI.
    if (has_cli_import) {
        std::erase_if(candidates, [](const AuthProfile* p) {
            return p->source == "env";
        });
    }

    // Session pin check
    if (!session_pin_.empty()) {
        for (const auto* c : candidates) {
            if (c->id == session_pin_) {
                ResolvedAuth auth;
                auth.profile_id = c->id;
                auth.provider = c->provider;
                auth.token = c->type == ProfileType::OAuth ? c->access_token : c->api_key;
                auth.is_oauth = c->type == ProfileType::OAuth;
                return auth;
            }
        }
    }

    // Sort: CLI Imports > other OAuth > ApiKey > everything else
    std::sort(candidates.begin(), candidates.end(),
        [](const AuthProfile* a, const AuthProfile* b) {
            bool a_cli = (a->source == "claude_code_import" || a->source == "gemini_import");
            bool b_cli = (b->source == "claude_code_import" || b->source == "gemini_import");
            if (a_cli != b_cli) return a_cli;

            // OAuth > ApiKey
            if (a->type != b->type) return a->type == ProfileType::OAuth;
            
            // Oldest used first
            return a->last_used_at < b->last_used_at;
        });

    const auto* best = candidates.front();
    ResolvedAuth auth;
    auth.profile_id = best->id;
    auth.provider = best->provider;
    auth.token = best->type == ProfileType::OAuth ? best->access_token : best->api_key;
    auth.is_oauth = best->type == ProfileType::OAuth;
    return auth;
}

auto AuthProfileStore::is_cooled_down(const std::string& id) const -> bool {
    auto it = cooldowns_.find(id);
    if (it == cooldowns_.end()) return false;
    return now_ms() < it->second.until_ms;
}

auto AuthProfileStore::now_ms() const -> int64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

auto AuthProfileStore::compute_cooldown_ms(CooldownReason reason, int consecutive) const
    -> int64_t {
    if (reason == CooldownReason::BillingError) {
        // 5h → 10h → 20h → 24h cap
        constexpr int64_t base = 5LL * 3600 * 1000;
        constexpr int64_t cap = 24LL * 3600 * 1000;
        int64_t ms = base;
        for (int i = 1; i < consecutive; ++i) ms *= 2;
        return std::min(ms, cap);
    }

    if (reason == CooldownReason::UnsupportedMethod) {
        // Permanent-ish failure for direct API (e.g. Claude Code token)
        return 24LL * 3600 * 1000; // 24 hours
    }

    // RateLimit / AuthError: 1m → 5m → 25m → 60m cap
    constexpr int64_t base = 60LL * 1000;  // 1 minute
    constexpr int64_t cap = 60LL * 60 * 1000;  // 60 minutes
    int64_t ms = base;
    for (int i = 1; i < consecutive; ++i) ms *= 5;
    return std::min(ms, cap);
}

void AuthProfileStore::load_from_json(const nlohmann::json& j) {
    if (j.contains("profiles")) {
        profiles_.clear();
        for (const auto& pj : j["profiles"]) {
            AuthProfile p;
            p.id = pj.value("id", "");
            p.provider = pj.value("provider", "");
            p.type = pj.value("type", "") == "oauth" ? ProfileType::OAuth : ProfileType::ApiKey;
            p.label = pj.value("label", "");
            p.api_key = pj.value("api_key", "");
            p.access_token = pj.value("access_token", "");
            p.refresh_token = pj.value("refresh_token", "");
            p.email = pj.value("email", "");
            p.expires_at = pj.value("expires_at", int64_t{0});
            p.source = pj.value("source", "");
            p.last_used_at = pj.value("last_used_at", int64_t{0});
            profiles_.push_back(std::move(p));
        }
    }

    if (j.contains("fallback_chains")) {
        for (auto& [key, val] : j["fallback_chains"].items()) {
            fallback_chains_[key] = val.get<std::vector<std::string>>();
        }
    }

    if (j.contains("cooldowns")) {
        for (auto& [key, val] : j["cooldowns"].items()) {
            CooldownEntry entry;
            entry.until_ms = val.value("until_ms", int64_t{0});
            entry.consecutive = val.value("consecutive", 0);
            auto reason_str = val.value("reason", "rate_limit");
            if (reason_str == "auth") entry.reason = CooldownReason::AuthError;
            else if (reason_str == "billing") entry.reason = CooldownReason::BillingError;
            else if (reason_str == "unsupported_method") entry.reason = CooldownReason::UnsupportedMethod;
            else entry.reason = CooldownReason::RateLimit;
            cooldowns_[key] = entry;
        }
    }
}

auto AuthProfileStore::to_json() const -> nlohmann::json {
    nlohmann::json j;
    j["version"] = 1;

    j["profiles"] = nlohmann::json::array();
    for (const auto& p : profiles_) {
        // Don't persist env-sourced profiles (they're ephemeral)
        if (p.source == "env") continue;

        nlohmann::json pj;
        pj["id"] = p.id;
        pj["provider"] = p.provider;
        pj["type"] = p.type == ProfileType::OAuth ? "oauth" : "api_key";
        pj["label"] = p.label;
        if (!p.api_key.empty()) pj["api_key"] = p.api_key;
        if (!p.access_token.empty()) pj["access_token"] = p.access_token;
        if (!p.refresh_token.empty()) pj["refresh_token"] = p.refresh_token;
        if (!p.email.empty()) pj["email"] = p.email;
        if (p.expires_at > 0) pj["expires_at"] = p.expires_at;
        pj["source"] = p.source;
        pj["last_used_at"] = p.last_used_at;
        j["profiles"].push_back(pj);
    }

    j["fallback_chains"] = nlohmann::json::object();
    for (const auto& [key, chain] : fallback_chains_) {
        j["fallback_chains"][key] = chain;
    }

    j["cooldowns"] = nlohmann::json::object();
    for (const auto& [key, entry] : cooldowns_) {
        nlohmann::json cj;
        cj["until_ms"] = entry.until_ms;
        cj["consecutive"] = entry.consecutive;
        switch (entry.reason) {
            case CooldownReason::AuthError: cj["reason"] = "auth"; break;
            case CooldownReason::BillingError: cj["reason"] = "billing"; break;
            case CooldownReason::UnsupportedMethod: cj["reason"] = "unsupported_method"; break;
            default: cj["reason"] = "rate_limit"; break;
        }
        j["cooldowns"][key] = cj;
    }

    return j;
}

} // namespace euxis::cli
