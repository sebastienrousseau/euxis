#include "euxis/scripts/tech_debt.hpp"

#include <fstream>
#include <string>

namespace euxis::scripts {
namespace {

constexpr int kMaxPragmas = 183;
constexpr int kMaxTodos = 13;

const std::vector<std::string> kSearchDirs{
    "euxis-adapters", "euxis-cli", "euxis-core", "euxis-crypto",
    "euxis-gateway", "euxis-metrics", "euxis-runtime", "euxis-security",
    "euxis-tui",
};

bool is_scannable(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    return ext == ".py" || ext == ".sh" || ext == ".cpp" || ext == ".hpp";
}

bool should_skip(const std::string& path_str) {
    return path_str.find("tests") != std::string::npos ||
           path_str.find(".venv") != std::string::npos ||
           path_str.find("egg-info") != std::string::npos;
}

} // namespace

auto DebtReport::to_json() const -> nlohmann::json {
    return {
        {"pragma_count", pragma_count},
        {"todo_count", todo_count},
        {"pragma_limit", pragma_limit},
        {"todo_limit", todo_limit},
        {"ok", ok},
    };
}

auto check_tech_debt(const std::filesystem::path& repo_root) -> DebtReport {
    DebtReport report;
    report.pragma_limit = kMaxPragmas;
    report.todo_limit = kMaxTodos;

    for (const auto& dir_name : kSearchDirs) {
        auto target = repo_root / dir_name / "src";
        if (!std::filesystem::exists(target)) {
            target = repo_root / dir_name;
            if (!std::filesystem::exists(target)) continue;
        }

        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(
                 target, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (!is_scannable(entry.path())) continue;
            if (should_skip(entry.path().string())) continue;

            std::ifstream file(entry.path());
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("# pragma: no cover") != std::string::npos)
                    ++report.pragma_count;
                if (line.find("TODO") != std::string::npos ||
                    line.find("FIXME") != std::string::npos)
                    ++report.todo_count;
            }
        }
    }

    report.ok = report.pragma_count <= report.pragma_limit &&
                report.todo_count <= report.todo_limit;
    return report;
}

} // namespace euxis::scripts
