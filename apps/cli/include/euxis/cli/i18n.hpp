#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace euxis::cli::i18n {

/// Lightweight i18n for non-Qt CLI output.
/// Loads JSON translation catalogs from a translations directory.
/// Falls back to the original English string if no translation is found.
class Catalog {
public:
    static auto instance() -> Catalog& {
        static Catalog catalog;
        return catalog;
    }

    /// Load translations for a locale (e.g. "fr", "de", "ja").
    /// Reads from <translations_dir>/<locale>.json
    void load(const std::string& locale,
              const std::filesystem::path& translations_dir) {
        std::lock_guard lock(mutex_);
        locale_ = locale;
        messages_.clear();

        if (locale == "en" || locale.empty()) {
            return;
        }

        auto path = translations_dir / (locale + ".json");
        if (!std::filesystem::exists(path)) {
            return;
        }

        std::ifstream f(path);
        nlohmann::json j;
        try {
            f >> j;
            for (const auto& [key, val] : j.items()) {
                if (val.is_string()) {
                    messages_[key] = val.get<std::string>();
                }
            }
        } catch (const nlohmann::json::exception&) {
            // Malformed translation file — fall back to English
            (void)0;  // swallowed: best-effort
        }
    }

    /// Translate a string. Returns the translation if found, otherwise the original.
    [[nodiscard]] auto translate(const std::string& text) const -> std::string {
        std::lock_guard lock(mutex_);
        auto it = messages_.find(text);
        if (it != messages_.end()) {
            return it->second;
        }
        return text;
    }

    [[nodiscard]] auto current_locale() const -> std::string {
        std::lock_guard lock(mutex_);
        return locale_;
    }

private:
    Catalog() = default;
    mutable std::mutex mutex_;
    std::string locale_{"en"};
    std::unordered_map<std::string, std::string> messages_;
};

/// Shorthand translation function for CLI strings.
[[nodiscard]] inline auto tr(const std::string& text) -> std::string {
    return Catalog::instance().translate(text);
}

} // namespace euxis::cli::i18n
