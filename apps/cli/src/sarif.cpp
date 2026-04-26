#include "euxis/cli/sarif.hpp"

#include <algorithm>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace euxis::cli {

auto findings_to_sarif(
    const std::vector<SarifFinding>& findings,
    const std::string& tool_version) -> nlohmann::json {

    nlohmann::json sarif;
    sarif["$schema"] = "https://docs.oasis-open.org/sarif/sarif/v2.1.0/cos02/schemas/sarif-schema-2.1.0.json";
    sarif["version"] = "2.1.0";

    nlohmann::json run;
    run["tool"]["driver"]["name"] = "euxis";
    run["tool"]["driver"]["version"] = tool_version;
    run["tool"]["driver"]["informationUri"] = "https://github.com/euxis-project/euxis";

    // Collect unique rules
    std::unordered_set<std::string> seen_rules;
    nlohmann::json rules = nlohmann::json::array();
    for (const auto& f : findings) {
        if (seen_rules.insert(f.rule_id).second) {
            rules.push_back({
                {"id", f.rule_id},
                {"shortDescription", {{"text", f.pillar + " finding from " + f.agent_id}}}
            });
        }
    }
    run["tool"]["driver"]["rules"] = rules;

    // Convert findings to results
    nlohmann::json results = nlohmann::json::array();
    for (const auto& f : findings) {
        nlohmann::json result;
        result["ruleId"] = f.rule_id;
        result["message"]["text"] = f.message;
        result["level"] = f.level;

        if (!f.file_path.empty()) {
            nlohmann::json location;
            location["physicalLocation"]["artifactLocation"]["uri"] = f.file_path;
            if (f.line > 0) {
                location["physicalLocation"]["region"]["startLine"] = f.line;
            }
            result["locations"] = nlohmann::json::array({location});
        }

        // Custom properties for euxis-specific metadata
        result["properties"] = {
            {"agent_id", f.agent_id},
            {"pillar", f.pillar}
        };

        results.push_back(result);
    }
    run["results"] = results;

    sarif["runs"] = nlohmann::json::array({run});
    return sarif;
}

auto extract_sarif_findings(
    const std::string& agent_output,
    const std::string& agent_id,
    const std::string& pillar,
    const std::string& verdict) -> std::vector<SarifFinding> {

    std::vector<SarifFinding> findings;
    std::istringstream stream(agent_output);
    std::string line;
    int finding_idx = 0;

    // Pattern: "file.cpp:123: message" or "Finding: message"
    static const std::regex file_line_re(R"(([a-zA-Z0-9_./-]+\.[a-zA-Z]{1,5}):(\d+)[:]\s*(.*))");
    static const std::regex finding_re(R"((?:Finding|Issue|Critical|Recommendation):\s*(.*))");

    std::string level = "warning";
    if (verdict == "FAILED") level = "error";
    else if (verdict == "PASS") level = "note";

    while (std::getline(stream, line)) {
        std::smatch m;
        if (std::regex_search(line, m, file_line_re)) {
            finding_idx++;
            findings.push_back({
                "euxis/" + pillar + "-" + std::to_string(finding_idx),
                m[3].str(),
                level,
                m[1].str(),
                std::stoi(m[2].str()),
                agent_id,
                pillar
            });
        } else if (std::regex_search(line, m, finding_re)) {
            std::string msg = m[1].str();
            if (msg.size() > 10 && msg.size() < 300) {
                finding_idx++;
                findings.push_back({
                    "euxis/" + pillar + "-" + std::to_string(finding_idx),
                    msg,
                    level,
                    "",
                    0,
                    agent_id,
                    pillar
                });
            }
        }
    }

    return findings;
}

} // namespace euxis::cli
