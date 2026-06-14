#include "euxis/cli/sarif.hpp"

#include <algorithm>
#include <chrono>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace euxis::cli {

namespace {

constexpr const char* kSarifSchema =
    "https://docs.oasis-open.org/sarif/sarif/v2.1.0/cos02/schemas/sarif-schema-2.1.0.json";
constexpr const char* kSarifVersion = "2.1.0";
constexpr const char* kToolInformationUri = "https://github.com/sebastienrousseau/euxis";

// SARIF taxonomy identifiers. URIs are anchored to the canonical
// taxonomy publications; SARIF consumers (GitHub Code Scanning,
// Defender, Snyk) follow these to render the taxa pivot.
constexpr const char* kCweTaxonomyName = "CWE";
constexpr const char* kCweTaxonomyUri  = "https://cwe.mitre.org/data/published.html";
constexpr const char* kOwaspTaxonomyName = "OWASP-Top-10-2025";
constexpr const char* kOwaspTaxonomyUri  = "https://owasp.org/Top10/2025/";

// ----- Canonical (Finding) path -----------------------------------------

void append_location(nlohmann::json& result,
                     const euxis::security::SourceLocation& loc) {
    if (loc.path.empty()) return;
    nlohmann::json location;
    location["physicalLocation"]["artifactLocation"]["uri"] = loc.path;
    if (loc.start_line > 0) {
        auto& region = location["physicalLocation"]["region"];
        region["startLine"] = loc.start_line;
        if (loc.start_column > 0) region["startColumn"] = loc.start_column;
        if (loc.end_line   > 0)   region["endLine"]     = loc.end_line;
        if (loc.end_column > 0)   region["endColumn"]   = loc.end_column;
        if (!loc.snippet.empty()) {
            region["snippet"]["text"] = loc.snippet;
        }
    }
    if (!result.contains("locations")) {
        result["locations"] = nlohmann::json::array();
    }
    result["locations"].push_back(location);
}

void append_fix(nlohmann::json& result,
                const euxis::security::FixSuggestion& fix) {
    nlohmann::json artifact_change;
    artifact_change["artifactLocation"]["uri"] = fix.replacement_range.path;

    nlohmann::json replacement;
    if (fix.replacement_range.start_line > 0) {
        auto& region = replacement["deletedRegion"];
        region["startLine"] = fix.replacement_range.start_line;
        if (fix.replacement_range.start_column > 0)
            region["startColumn"] = fix.replacement_range.start_column;
        if (fix.replacement_range.end_line   > 0)
            region["endLine"]     = fix.replacement_range.end_line;
        if (fix.replacement_range.end_column > 0)
            region["endColumn"]   = fix.replacement_range.end_column;
    }
    replacement["insertedContent"]["text"] = fix.replacement_text;
    artifact_change["replacements"] = nlohmann::json::array({replacement});

    nlohmann::json sarif_fix;
    sarif_fix["description"]["text"] = fix.description;
    sarif_fix["artifactChanges"] = nlohmann::json::array({artifact_change});
    sarif_fix["properties"]["deterministic"] = fix.deterministic;

    if (!result.contains("fixes")) {
        result["fixes"] = nlohmann::json::array();
    }
    result["fixes"].push_back(sarif_fix);
}

nlohmann::json build_cwe_taxonomy(
    const std::vector<euxis::security::CweRef>& cwes) {
    nlohmann::json tax;
    tax["name"] = kCweTaxonomyName;
    tax["informationUri"] = kCweTaxonomyUri;
    tax["organization"] = "MITRE";
    tax["shortDescription"]["text"] = "Common Weakness Enumeration";

    nlohmann::json taxa = nlohmann::json::array();
    std::unordered_set<std::string> seen;
    for (const auto& c : cwes) {
        if (c.id.empty() || !seen.insert(c.id).second) continue;
        nlohmann::json t;
        t["id"] = c.id;
        t["name"] = c.short_name;
        t["properties"]["in_top_25_2025"] = c.in_top_25_2025;
        if (c.in_top_25_2025 && c.top_25_rank > 0) {
            t["properties"]["top_25_rank"] = c.top_25_rank;
        }
        taxa.push_back(t);
    }
    tax["taxa"] = taxa;
    return tax;
}

nlohmann::json build_owasp_taxonomy(
    const std::vector<euxis::security::OwaspCategory>& cats) {
    nlohmann::json tax;
    tax["name"] = kOwaspTaxonomyName;
    tax["informationUri"] = kOwaspTaxonomyUri;
    tax["organization"] = "OWASP";
    tax["shortDescription"]["text"] = "OWASP Top 10 (2025 release)";

    nlohmann::json taxa = nlohmann::json::array();
    std::unordered_set<std::string> seen;
    for (auto c : cats) {
        const char* id = euxis::security::owasp_id(c);
        if (id == nullptr || *id == '\0' || !seen.insert(id).second) continue;
        nlohmann::json t;
        t["id"] = id;
        t["name"] = euxis::security::owasp_label(c);
        taxa.push_back(t);
    }
    tax["taxa"] = taxa;
    return tax;
}

} // namespace

// ============================================================================
// Canonical Finding path
// ============================================================================

auto findings_to_sarif(
    const std::vector<euxis::security::Finding>& findings,
    const std::string& tool_version) -> nlohmann::json {

    nlohmann::json sarif;
    sarif["$schema"] = kSarifSchema;
    sarif["version"] = kSarifVersion;

    nlohmann::json run;
    run["tool"]["driver"]["name"] = "euxis";
    run["tool"]["driver"]["version"] = tool_version;
    run["tool"]["driver"]["informationUri"] = kToolInformationUri;
    run["tool"]["driver"]["semanticVersion"] = tool_version;

    // Build rules array (deduplicated by rule_id).
    std::unordered_set<std::string> seen_rules;
    nlohmann::json rules = nlohmann::json::array();
    std::vector<euxis::security::CweRef> seen_cwes;
    std::vector<euxis::security::OwaspCategory> seen_owasp;

    for (const auto& f : findings) {
        if (seen_rules.insert(f.rule_id).second) {
            nlohmann::json rule;
            rule["id"] = f.rule_id;
            rule["name"] = f.rule_id;
            rule["shortDescription"]["text"] = f.message;
            rule["defaultConfiguration"]["level"] =
                euxis::security::sarif_level(f.severity);

            // Rule-level relationships to CWE / OWASP taxa.
            nlohmann::json relationships = nlohmann::json::array();
            if (f.cwe.has_value() && !f.cwe->id.empty()) {
                nlohmann::json rel;
                rel["target"]["id"] = f.cwe->id;
                rel["target"]["toolComponent"]["name"] = kCweTaxonomyName;
                rel["kinds"] = nlohmann::json::array({"relevant"});
                relationships.push_back(rel);
            }
            if (f.owasp != euxis::security::OwaspCategory::None) {
                nlohmann::json rel;
                rel["target"]["id"] = euxis::security::owasp_id(f.owasp);
                rel["target"]["toolComponent"]["name"] = kOwaspTaxonomyName;
                rel["kinds"] = nlohmann::json::array({"relevant"});
                relationships.push_back(rel);
            }
            if (!relationships.empty()) {
                rule["relationships"] = relationships;
            }
            rules.push_back(rule);
        }

        if (f.cwe.has_value() && !f.cwe->id.empty()) {
            seen_cwes.push_back(*f.cwe);
        }
        if (f.owasp != euxis::security::OwaspCategory::None) {
            seen_owasp.push_back(f.owasp);
        }
    }
    run["tool"]["driver"]["rules"] = rules;

    // Taxonomies block (SARIF v2.1.0 §3.19).
    nlohmann::json taxonomies = nlohmann::json::array();
    if (!seen_cwes.empty()) {
        taxonomies.push_back(build_cwe_taxonomy(seen_cwes));
    }
    if (!seen_owasp.empty()) {
        taxonomies.push_back(build_owasp_taxonomy(seen_owasp));
    }
    if (!taxonomies.empty()) {
        run["taxonomies"] = taxonomies;
    }

    // Results.
    nlohmann::json results = nlohmann::json::array();
    for (const auto& f : findings) {
        nlohmann::json result;
        result["ruleId"] = f.rule_id;
        result["message"]["text"] = f.message;
        result["level"] = euxis::security::sarif_level(f.severity);

        // Stable fingerprint for baseline-file matching (SARIF §3.27.16).
        if (!f.stable_fingerprint.empty()) {
            result["fingerprints"]["euxis.stable.v1"] = f.stable_fingerprint;
            result["partialFingerprints"]["euxis.stable.v1"] = f.stable_fingerprint;
        }

        append_location(result, f.primary_location);
        for (const auto& loc : f.related_locations) {
            append_location(result, loc);
        }

        for (const auto& fix : f.fixes) {
            append_fix(result, fix);
        }

        // Properties — euxis-specific fields the consumer can pivot on.
        nlohmann::json props;
        props["severity"]   = euxis::security::severity_label(f.severity);
        props["confidence"] = static_cast<int>(f.confidence);
        props["source"]     = static_cast<int>(f.source);
        if (f.quantum != euxis::security::QuantumDeprecation::None) {
            props["quantum_deprecation"] =
                euxis::security::quantum_label(f.quantum);
        }
        if (!f.compliance_taxa.empty()) {
            props["compliance_taxa"] = f.compliance_taxa;
        }
        if (!f.ensemble_votes.empty()) {
            nlohmann::json votes = nlohmann::json::array();
            for (const auto& v : f.ensemble_votes) {
                votes.push_back({
                    {"provider", v.provider},
                    {"true_positive", v.true_positive},
                    {"confidence", v.confidence},
                });
            }
            props["ensemble_votes"] = votes;
        }
        result["properties"] = props;

        results.push_back(result);
    }
    run["results"] = results;

    sarif["runs"] = nlohmann::json::array({run});
    return sarif;
}

// ============================================================================
// Legacy (SarifFinding) path — preserved for agent-driven verifications
// ============================================================================

auto findings_to_sarif(
    const std::vector<SarifFinding>& findings,
    const std::string& tool_version) -> nlohmann::json {

    nlohmann::json sarif;
    sarif["$schema"] = kSarifSchema;
    sarif["version"] = kSarifVersion;

    nlohmann::json run;
    run["tool"]["driver"]["name"] = "euxis";
    run["tool"]["driver"]["version"] = tool_version;
    run["tool"]["driver"]["informationUri"] = kToolInformationUri;

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
