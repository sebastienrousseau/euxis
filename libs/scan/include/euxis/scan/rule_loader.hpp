/// @file
/// @brief Parse OpenGrep YAML rule packs into typed RulePack objects.
#pragma once

#include <expected>
#include <filesystem>
#include <string_view>

#include "euxis/scan/rule.hpp"

namespace euxis::scan {

struct LoadError {
    std::string message;
    std::filesystem::path file;
    int line{0};
};

/// Parse a YAML document containing an OpenGrep rule pack
/// (`rules: [...]` at the top level). `name` is recorded on the
/// returned RulePack for diagnostics.
[[nodiscard]] auto load_rules_yaml(std::string_view yaml,
                                   std::string name = "anonymous")
    -> std::expected<RulePack, LoadError>;

/// Convenience: read `path` and dispatch to `load_rules_yaml`.
/// The RulePack's name is set to `path.filename()`.
[[nodiscard]] auto load_rules_file(const std::filesystem::path& path)
    -> std::expected<RulePack, LoadError>;

} // namespace euxis::scan
