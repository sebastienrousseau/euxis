#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::scripts {

/// Return sorted doc files (.md, .rst, .yaml, .yml) under docs/.
[[nodiscard]] auto docs_source_files(const std::filesystem::path& repo_root)
    -> std::vector<std::filesystem::path>;

/// Return required docs that do not exist.
[[nodiscard]] auto missing_required_docs(const std::filesystem::path& repo_root)
    -> std::vector<std::filesystem::path>;

/// Map a package name to docs/modules/<name>.md.
[[nodiscard]] auto module_doc_path(const std::string& package_name)
    -> std::filesystem::path;

/// Return package names missing a docs/modules/ page.
[[nodiscard]] auto missing_module_docs(
    const std::filesystem::path& repo_root,
    const std::vector<std::string>& package_names) -> std::vector<std::string>;

/// Build a compact docs quality report.
[[nodiscard]] auto build_docs_quality_report(
    const std::filesystem::path& repo_root,
    const std::vector<std::string>& package_names) -> nlohmann::json;

} // namespace euxis::scripts
