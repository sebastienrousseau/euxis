#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::cli {

enum class ProfileType { ApiKey, OAuth };
enum class CooldownReason { RateLimit, AuthError, BillingError };

struct AuthProfile {
    std::string id;        // "anthropic:user@example.com" or "openai:default"
    std::string provider;  // "anthropic", "openai", "gemini"
    ProfileType type{ProfileType::ApiKey};
    std::string label;

    // ApiKey fields
    std::string api_key;

    // OAuth fields
    std::string access_token;
    std::string refresh_token;
    std::string email;
    int64_t expires_at{0};  // ms epoch

    std::string source;     // "manual", "claude_code_import", "gemini_import", "browser_oauth", "env"
    int64_t last_used_at{0};
};

struct ResolvedAuth {
    std::string profile_id;
    std::string provider;
    std::string token;
    bool is_oauth{false};  // true → Bearer, false → x-api-key
};

/// Thread-safe auth profile store with rotation, cooldowns, and fallback chains.
class AuthProfileStore {
public:
    explicit AuthProfileStore(const std::string& data_dir);

    // CRUD
    void add_api_key(const std::string& provider, const std::string& key,
                     const std::string& label = {});
    void add_oauth(const std::string& provider, const std::string& access_token,
                   const std::string& refresh_token, int64_t expires_at,
                   const std::string& email = {});
    void remove_profile(const std::string& id);
    [[nodiscard]] auto profiles_for(const std::string& provider) const -> std::vector<AuthProfile>;
    [[nodiscard]] auto all_profiles() const -> std::vector<AuthProfile>;

    // Resolution
    [[nodiscard]] auto resolve(const std::string& provider) -> std::optional<ResolvedAuth>;
    [[nodiscard]] auto resolve_with_fallback(const std::string& provider) -> std::optional<ResolvedAuth>;

    // Session pinning
    void pin_session(const std::string& profile_id);
    void clear_session_pin();

    // Cooldowns
    void report_failure(const std::string& profile_id, CooldownReason reason);
    void report_success(const std::string& profile_id);

    // Auto-import
    void auto_import_claude_oauth();   // ~/.claude/.credentials.json
    void auto_import_gemini_oauth();   // ~/.gemini/oauth_creds.json
    void import_env_vars();            // ANTHROPIC_API_KEY, OPENAI_API_KEY, GEMINI_API_KEY

    // Persistence
    void save();
    void reload();

    // Fallback chain access
    [[nodiscard]] auto fallback_chain(const std::string& provider) const -> std::vector<std::string>;
    void set_fallback_chain(const std::string& provider, const std::vector<std::string>& chain);

    // Cooldown query
    [[nodiscard]] auto cooldown_remaining_ms(const std::string& profile_id) const -> int64_t;

private:
    std::string data_dir_;
    std::string store_path_;
    mutable std::mutex mutex_;

    std::vector<AuthProfile> profiles_;
    std::map<std::string, std::vector<std::string>> fallback_chains_;

    // Cooldown state
    struct CooldownEntry {
        int64_t until_ms{0};    // cooldown active until (ms epoch)
        int consecutive{0};     // consecutive failures
        CooldownReason reason{CooldownReason::RateLimit};
    };
    std::map<std::string, CooldownEntry> cooldowns_;

    std::string session_pin_;

    [[nodiscard]] auto pick_best(const std::string& provider) const -> std::optional<ResolvedAuth>;
    [[nodiscard]] auto is_cooled_down(const std::string& id) const -> bool;
    [[nodiscard]] auto now_ms() const -> int64_t;
    [[nodiscard]] auto compute_cooldown_ms(CooldownReason reason, int consecutive) const -> int64_t;

    void load_from_json(const nlohmann::json& j);
    [[nodiscard]] auto to_json() const -> nlohmann::json;
};

} // namespace euxis::cli
