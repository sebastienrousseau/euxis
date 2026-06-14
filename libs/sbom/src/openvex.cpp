#include "euxis/sbom/openvex.hpp"
#include "euxis/sbom/types.hpp"

namespace euxis::sbom {

auto vex_status_str(VexStatus s) noexcept -> const char* {
    switch (s) {
        case VexStatus::NotAffected:        return "not_affected";
        case VexStatus::Affected:           return "affected";
        case VexStatus::Fixed:              return "fixed";
        case VexStatus::UnderInvestigation: return "under_investigation";
    }
    return "under_investigation";
}

auto vex_justification_str(VexJustification j) noexcept -> const char* {
    switch (j) {
        case VexJustification::None:
            return "";
        case VexJustification::ComponentNotPresent:
            return "component_not_present";
        case VexJustification::VulnerableCodeNotPresent:
            return "vulnerable_code_not_present";
        case VexJustification::VulnerableCodeNotInExecutePath:
            return "vulnerable_code_not_in_execute_path";
        case VexJustification::VulnerableCodeCannotBeControlledByAdversary:
            return "vulnerable_code_cannot_be_controlled_by_adversary";
        case VexJustification::InlineMitigationsAlreadyExist:
            return "inline_mitigations_already_exist";
    }
    return "";
}

auto to_openvex(const VexDocument& doc) -> nlohmann::json {
    nlohmann::json out;
    out["@context"] = "https://openvex.dev/ns/v0.2.0";
    out["@id"]      = doc.id.empty() ? generate_serial_number() : doc.id;
    out["author"]   = doc.author;
    if (!doc.author_role.empty()) out["role"] = doc.author_role;
    out["timestamp"] = to_rfc3339(doc.timestamp);
    out["version"]   = 1;
    out["tooling"]   = "euxis";

    nlohmann::json statements = nlohmann::json::array();
    for (const auto& s : doc.statements) {
        nlohmann::json stmt;
        stmt["vulnerability"] = {{"name", s.vulnerability_name}};

        nlohmann::json products = nlohmann::json::array();
        for (const auto& p : s.product_purls) {
            products.push_back({{"@id", p}});
        }
        stmt["products"] = products;

        stmt["status"] = vex_status_str(s.status);
        if (s.status == VexStatus::NotAffected &&
            s.justification != VexJustification::None) {
            stmt["justification"] = vex_justification_str(s.justification);
        }
        if (!s.impact_statement.empty()) {
            stmt["impact_statement"] = s.impact_statement;
        }
        if (s.status == VexStatus::Fixed && !s.action_statement.empty()) {
            stmt["action_statement"] = s.action_statement;
        }
        stmt["timestamp"] = to_rfc3339(s.timestamp);
        statements.push_back(stmt);
    }
    out["statements"] = statements;
    return out;
}

} // namespace euxis::sbom
