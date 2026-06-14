#include "euxis/sbom/cyclonedx.hpp"

namespace euxis::sbom {

namespace {

nlohmann::json hash_to_json(const Hash& h) {
    return {
        {"alg",     hash_alg_cyclonedx(h.algorithm)},
        {"content", h.value},
    };
}

nlohmann::json license_to_json(const License& l) {
    nlohmann::json out;
    if (!l.spdx_expression.empty()) {
        out["expression"] = l.spdx_expression;
    }
    if (l.url) {
        out["license"]["url"] = *l.url;
    }
    return out;
}

nlohmann::json external_ref_to_json(const ExternalRef& r) {
    nlohmann::json out{
        {"type", external_ref_type_str(r.type)},
        {"url",  r.url},
    };
    if (!r.comment.empty()) out["comment"] = r.comment;
    return out;
}

nlohmann::json component_to_json(const Component& c) {
    nlohmann::json out;
    out["type"]    = component_type_str(c.type);
    out["bom-ref"] = c.bom_ref.empty() ? c.purl : c.bom_ref;
    out["name"]    = c.name;
    if (!c.version.empty())     out["version"]     = c.version;
    if (!c.publisher.empty())   out["publisher"]   = c.publisher;
    if (!c.supplier.empty())    out["supplier"]["name"] = c.supplier;
    if (!c.description.empty()) out["description"] = c.description;
    if (!c.purl.empty())        out["purl"]        = c.purl;
    if (!c.cpe.empty())         out["cpe"]         = c.cpe;

    if (!c.licenses.empty()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& l : c.licenses) arr.push_back(license_to_json(l));
        out["licenses"] = arr;
    }
    if (!c.hashes.empty()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& h : c.hashes) arr.push_back(hash_to_json(h));
        out["hashes"] = arr;
    }
    if (!c.external_refs.empty()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : c.external_refs) arr.push_back(external_ref_to_json(r));
        out["externalReferences"] = arr;
    }
    if (!c.properties.empty()) {
        nlohmann::json arr = nlohmann::json::array();
        // CycloneDX prescribes ordered key/value array.
        for (const auto& [k, v] : c.properties) {
            arr.push_back({{"name", k}, {"value", v}});
        }
        out["properties"] = arr;
    }
    return out;
}

nlohmann::json dependency_to_json(const Dependency& d) {
    nlohmann::json out{{"ref", d.ref}};
    if (!d.depends_on.empty()) {
        out["dependsOn"] = d.depends_on;
    }
    return out;
}

} // namespace

auto to_cyclonedx_1_6(const SbomDocument& doc) -> nlohmann::json {
    nlohmann::json bom;
    bom["bomFormat"]    = "CycloneDX";
    bom["specVersion"]  = "1.6";
    bom["version"]      = 1;
    bom["serialNumber"] =
        doc.serial_number.empty() ? generate_serial_number() : doc.serial_number;

    nlohmann::json metadata;
    metadata["timestamp"] = to_rfc3339(doc.created_at);

    if (!doc.tools.empty()) {
        nlohmann::json comps = nlohmann::json::array();
        for (const auto& t : doc.tools) {
            comps.push_back({
                {"type",    "application"},
                {"vendor",  t.vendor},
                {"name",    t.name},
                {"version", t.version},
            });
        }
        // CycloneDX 1.6 §metadata.tools.components is the modern
        // shape; the deprecated array form is still accepted.
        metadata["tools"]["components"] = comps;
    }
    if (doc.root) {
        metadata["component"] = component_to_json(*doc.root);
    }
    if (!doc.properties.empty()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& [k, v] : doc.properties) {
            arr.push_back({{"name", k}, {"value", v}});
        }
        metadata["properties"] = arr;
    }
    bom["metadata"] = metadata;

    nlohmann::json comps = nlohmann::json::array();
    for (const auto& c : doc.components) comps.push_back(component_to_json(c));
    bom["components"] = comps;

    if (!doc.dependencies.empty()) {
        nlohmann::json deps = nlohmann::json::array();
        for (const auto& d : doc.dependencies) deps.push_back(dependency_to_json(d));
        bom["dependencies"] = deps;
    }

    return bom;
}

} // namespace euxis::sbom
