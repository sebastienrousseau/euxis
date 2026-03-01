#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "runner.hpp"

namespace euxis::bench {

class BenchmarkReporter {
public:
    [[nodiscard]] static auto to_json(const std::vector<SuiteReport>& reports) -> nlohmann::json;
    [[nodiscard]] static auto to_markdown(const std::vector<SuiteReport>& reports) -> std::string;
    static void write_json(const std::vector<SuiteReport>& reports, const std::filesystem::path& path);
    static void write_markdown(const std::vector<SuiteReport>& reports, const std::filesystem::path& path);
};

} // namespace euxis::bench
