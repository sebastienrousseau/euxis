#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::scripts {

struct DebtReport {
    int pragma_count{0};
    int todo_count{0};
    int pragma_limit{183};
    int todo_limit{13};
    bool ok{true};

    [[nodiscard]] auto to_json() const -> nlohmann::json;
};

/// Scan source directories for pragma/TODO counts.
[[nodiscard]] auto check_tech_debt(const std::filesystem::path& repo_root)
    -> DebtReport;

} // namespace euxis::scripts
