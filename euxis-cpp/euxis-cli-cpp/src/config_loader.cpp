#include "euxis/cli/config_loader.hpp"

#include <filesystem>
#include <fstream>

#include <spdlog/spdlog.h>

namespace euxis::cli {

ConfigLoader::ConfigLoader(const std::string& data_dir) : data_dir_(data_dir) {}

auto ConfigLoader::load(const std::string& relative_path) const
    -> std::optional<nlohmann::json> {
    auto path = std::filesystem::path(data_dir_) / relative_path;
    if (!std::filesystem::exists(path)) {
        spdlog::debug("Config not found: {}", path.string());
        return std::nullopt;
    }
    try {
        std::ifstream f(path);
        if (!f.is_open()) return std::nullopt;
        return nlohmann::json::parse(f);
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::warn("Failed to parse {}: {}", path.string(), e.what());
        return std::nullopt;
    }
}

auto ConfigLoader::load_or(const std::string& relative_path,
                           const nlohmann::json& fallback) const -> nlohmann::json {
    auto result = load(relative_path);
    return result.value_or(fallback);
}

auto ConfigLoader::exists(const std::string& relative_path) const -> bool {
    return std::filesystem::exists(std::filesystem::path(data_dir_) / relative_path);
}

auto ConfigLoader::data_dir() const -> const std::string& {
    return data_dir_;
}

} // namespace euxis::cli
