#include "euxis/sbom/spdx.hpp"

#include <string>

namespace euxis::sbom {

namespace {

/// SPDX-Identifier shape required by SPDX 3.0.1 §SPDXID. Already
/// constructable from `Component::bom_ref` so we don't churn the
/// canonical type.
auto spdxid(const std::string& bom_ref) -> std::string {
    std::string out;
    out.reserve(bom_ref.size() + 8);
    out.append("SPDXRef-");
    for (char c : bom_ref) {
        // SPDX 3.0.1 §SPDXID allows [a-zA-Z0-9.-]. Replace anything
        // else with a hyphen for spec compliance.
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-') {
            out.push_back(c);
        } else {
            out.push_back('-');
        }
    }
    return out;
}

nlohmann::json package_to_node(const Component& c) {
    nlohmann::json node;
    node["@type"]      = "Package";
    node["spdxId"]     = spdxid(c.bom_ref.empty() ? (c.purl.empty() ? c.name : c.purl) : c.bom_ref);
    node["name"]       = c.name;
    if (!c.version.empty()) node["packageVersion"] = c.version;
    if (!c.supplier.empty()) {
        node["supplier"] = nlohmann::json{
            {"@type", "Organization"},
            {"name",  c.supplier},
        };
    }
    if (!c.publisher.empty()) {
        node["originator"] = nlohmann::json{
            {"@type", "Organization"},
            {"name",  c.publisher},
        };
    }
    if (!c.description.empty()) node["description"] = c.description;
    if (!c.purl.empty()) {
        node["externalIdentifier"] = nlohmann::json::array({
            {{"@type", "ExternalIdentifier"},
             {"externalIdentifierType", "purl"},
             {"identifier", c.purl}},
        });
    }

    if (!c.hashes.empty()) {
        nlohmann::json checks = nlohmann::json::array();
        for (const auto& h : c.hashes) {
            checks.push_back({
                {"@type",              "Hash"},
                {"checksumAlgorithm",  hash_alg_spdx(h.algorithm)},
                {"checksumValue",      h.value},
            });
        }
        node["verifiedUsing"] = checks;
    }

    if (!c.licenses.empty()) {
        const auto& l = c.licenses.front();
        if (!l.spdx_expression.empty()) {
            node["concludedLicense"] = l.spdx_expression;
        }
    }
    return node;
}

} // namespace

auto to_spdx_3_0_1(const SbomDocument& doc) -> nlohmann::json {
    nlohmann::json out;
    // SPDX 3.0 JSON-LD @context. Only the core + security profiles
    // are needed for SBOM + VEX emission.
    out["@context"] = nlohmann::json::array({
        "https://spdx.org/rdf/3.0.1/spdx-context.jsonld",
    });

    nlohmann::json graph = nlohmann::json::array();

    // CreationInfo
    nlohmann::json creation;
    creation["@type"]   = "CreationInfo";
    creation["spdxId"]  = "_:creationinfo";
    creation["created"] = to_rfc3339(doc.created_at);
    creation["specVersion"] = "3.0.1";

    nlohmann::json creators = nlohmann::json::array();
    for (const auto& t : doc.tools) {
        creators.push_back(nlohmann::json{
            {"@type",          "Tool"},
            {"name",           t.name},
            {"toolVersion",    t.version},
            {"creatorVendor",  t.vendor},
        });
    }
    if (!creators.empty()) creation["createdUsing"] = creators;
    graph.push_back(creation);

    // SpdxDocument
    nlohmann::json document;
    document["@type"]     = "SpdxDocument";
    document["spdxId"]    = doc.document_namespace + "/" +
        (doc.serial_number.empty() ? generate_serial_number()
                                   : doc.serial_number);
    document["name"]      = doc.root ? doc.root->name : std::string{"euxis-scan"};
    document["dataLicense"] = "CC0-1.0";
    document["creationInfo"] = "_:creationinfo";
    graph.push_back(document);

    // Packages
    if (doc.root) graph.push_back(package_to_node(*doc.root));
    for (const auto& c : doc.components) {
        graph.push_back(package_to_node(c));
    }

    // Relationships
    for (const auto& d : doc.dependencies) {
        for (const auto& to : d.depends_on) {
            graph.push_back({
                {"@type",            "Relationship"},
                {"relationshipType", "dependsOn"},
                {"from",             spdxid(d.ref)},
                {"to",               nlohmann::json::array({spdxid(to)})},
            });
        }
    }

    out["@graph"] = graph;
    return out;
}

} // namespace euxis::sbom
