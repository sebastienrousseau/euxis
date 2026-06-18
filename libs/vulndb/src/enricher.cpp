/// @file
/// @brief Cross-references SBOM components against OSV.

#include "euxis/vulndb/enricher.hpp"

#include <chrono>
#include <string>

namespace euxis::vulndb {

Enricher::Enricher(const OsvClient& client, EnricherConfig cfg)
    : client_{&client}, cfg_{cfg} {}

namespace {

[[nodiscard]] auto build_vex_statement(const VulnMatch& match,
                                       const OsvVuln& vuln)
    -> euxis::sbom::VexStatement {
    euxis::sbom::VexStatement st;
    st.vulnerability_name = vuln.id;
    st.status             = euxis::sbom::VexStatus::Affected;
    st.product_purls      = {match.purl};
    return st;
}

[[nodiscard]] auto build_not_affected(const VulnMatch& match)
    -> euxis::sbom::VexStatement {
    euxis::sbom::VexStatement st;
    st.vulnerability_name = "no-known-vulnerabilities";
    st.status             = euxis::sbom::VexStatus::NotAffected;
    st.justification      = euxis::sbom::VexJustification::ComponentNotPresent;
    st.product_purls      = {match.purl};
    return st;
}

} // namespace

auto Enricher::enrich(const euxis::sbom::SbomDocument& doc) const
    -> std::expected<EnrichedReport, Error> {
    EnrichedReport out;
    out.matches.reserve(doc.components.size());

    std::size_t processed = 0;
    for (const auto& comp : doc.components) {
        if (cfg_.max_components > 0 && processed >= cfg_.max_components) {
            break;
        }
        ++processed;

        VulnMatch match;
        match.purl              = comp.purl;
        match.component_name    = comp.name;
        match.component_version = comp.version;

        // Components without a PURL cannot be queried — record an empty
        // match so the caller still sees the row.
        if (!comp.purl.empty()) {
            auto result = client_->query_by_purl(comp.purl);
            if (!result) {
                // Surface the first transport failure; the caller may
                // wish to retry the whole enrichment or fall back to
                // the offline dump.
                return std::unexpected(result.error());
            }
            match.vulns = std::move(*result);
        }

        if (!match.vulns.empty()) {
            for (const auto& v : match.vulns) {
                out.vex.statements.push_back(build_vex_statement(match, v));
            }
        } else if (cfg_.emit_not_affected && !comp.purl.empty()) {
            out.vex.statements.push_back(build_not_affected(match));
        }

        out.matches.push_back(std::move(match));
    }
    return out;
}

} // namespace euxis::vulndb
