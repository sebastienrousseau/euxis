#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace euxis::cli {

/// Load JSON configuration files from data/.
class ConfigLoader {
public:
    explicit ConfigLoader(const std::string& data_dir);

    /// Load a JSON file by relative path under data_dir. Returns nullopt if missing.
    [[nodiscard]] auto load(const std::string& relative_path) const -> std::optional<nlohmann::json>;

    /// Load with fallback to a default JSON value.
    [[nodiscard]] auto load_or(const std::string& relative_path,
                 const nlohmann::json& fallback) const -> nlohmann::json;

    /// Check if a config file exists.
    [[nodiscard]] auto exists(const std::string& relative_path) const -> bool;

    /// Return the resolved data directory.
    [[nodiscard]] auto data_dir() const -> const std::string&;

private:
    std::string data_dir_;
};

} // namespace euxis::cli
