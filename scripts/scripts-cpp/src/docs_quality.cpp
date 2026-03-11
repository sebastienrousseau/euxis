#include "euxis/scripts/docs_quality.hpp"

#include <algorithm>
#include <set>

namespace euxis::scripts {
namespace {

const std::set<std::string> kDocExtensions{".md", ".rst", ".yaml", ".yml"};

const std::vector<std::filesystem::path> kRequiredDocs{
    "README.md",
    "docs/index.rst",
    "docs/modules/index.md",
    "docs/modules/euxis-docs.md",
};

} // namespace

auto docs_source_files(const std::filesystem::path& repo_root)
    -> std::vector<std::filesystem::path> {
    auto docs_dir = repo_root / "docs";
    if (!std::filesystem::is_directory(docs_dir)) return {};

    std::vector<std::filesystem::path> files;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(docs_dir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        std::ranges::transform(ext, ext.begin(),
                               [](unsigned char c) { return std::tolower(c); });
        if (kDocExtensions.contains(ext)) {
            files.push_back(std::filesystem::relative(entry.path(), repo_root));
        }
    }
    std::ranges::sort(files);
    return files;
}

auto missing_required_docs(const std::filesystem::path& repo_root)
    -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> missing;
    for (const auto& req : kRequiredDocs) {
        if (!std::filesystem::is_regular_file(repo_root / req)) {
            missing.push_back(req);
        }
    }
    std::ranges::sort(missing);
    return missing;
}

auto module_doc_path(const std::string& package_name) -> std::filesystem::path {
    std::string normalized = package_name;
    std::ranges::transform(normalized, normalized.begin(),
                           [](unsigned char c) { return std::tolower(c); });
    // Trim whitespace
    auto start = normalized.find_first_not_of(" \t");
    auto end = normalized.find_last_not_of(" \t");
    if (start != std::string::npos)
        normalized = normalized.substr(start, end - start + 1);

    if (!normalized.starts_with("euxis-"))
        normalized = "euxis-" + normalized;

    return std::filesystem::path("docs/modules") / (normalized + ".md");
}

auto missing_module_docs(const std::filesystem::path& repo_root,
                         const std::vector<std::string>& package_names)
    -> std::vector<std::string> {
    std::set<std::string> unique_names(package_names.begin(), package_names.end());
    std::vector<std::string> missing;
    for (const auto& name : unique_names) {
        auto doc = repo_root / module_doc_path(name);
        if (!std::filesystem::is_regular_file(doc)) {
            missing.push_back(name);
        }
    }
    return missing;
}

auto build_docs_quality_report(const std::filesystem::path& repo_root,
                               const std::vector<std::string>& package_names)
    -> nlohmann::json {
    auto source_files = docs_source_files(repo_root);
    auto missing_req = missing_required_docs(repo_root);
    auto missing_mods = missing_module_docs(repo_root, package_names);

    nlohmann::json missing_req_json = nlohmann::json::array();
    for (const auto& p : missing_req)
        missing_req_json.push_back(p.string());

    return {
        {"docs_file_count", static_cast<int>(source_files.size())},
        {"missing_required_docs", missing_req_json},
        {"missing_module_docs", missing_mods},
        {"ok", missing_req.empty() && missing_mods.empty()},
    };
}

} // namespace euxis::scripts
