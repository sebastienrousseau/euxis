#include "euxis/attest/slsa.hpp"

#include <ctime>

namespace euxis::attest {

namespace {

auto to_rfc3339(std::chrono::system_clock::time_point tp) -> std::string {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm gm{};
#if defined(_WIN32)
    gmtime_s(&gm, &t);
#else
    gmtime_r(&t, &gm);
#endif
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &gm);
    return std::string{buf};
}

auto resolved_dep_to_json(const ResolvedDependency& d) -> nlohmann::json {
    nlohmann::json out;
    out["uri"] = d.uri;
    nlohmann::json digest = nlohmann::json::object();
    for (const auto& [alg, val] : d.digest) digest[alg] = val;
    if (!digest.empty()) out["digest"] = digest;
    if (!d.name.empty()) out["name"] = d.name;
    return out;
}

} // namespace

auto to_json(const SlsaProvenance& p) -> nlohmann::json {
    nlohmann::json out;

    nlohmann::json bd;
    bd["buildType"]          = p.build_definition.build_type;
    bd["externalParameters"] = p.build_definition.external_parameters;
    bd["internalParameters"] = p.build_definition.internal_parameters;
    nlohmann::json deps = nlohmann::json::array();
    for (const auto& d : p.build_definition.resolved_dependencies) {
        deps.push_back(resolved_dep_to_json(d));
    }
    bd["resolvedDependencies"] = deps;
    out["buildDefinition"] = bd;

    nlohmann::json rd;
    nlohmann::json builder;
    builder["id"] = p.run_details.builder.id;
    if (!p.run_details.builder.version.empty()) {
        nlohmann::json v = nlohmann::json::object();
        for (const auto& [k, val] : p.run_details.builder.version) {
            v[k] = val;
        }
        builder["version"] = v;
    }
    if (!p.run_details.builder.builder_dependencies.empty()) {
        nlohmann::json bdeps = nlohmann::json::array();
        for (const auto& d : p.run_details.builder.builder_dependencies) {
            bdeps.push_back(resolved_dep_to_json(d));
        }
        builder["builderDependencies"] = bdeps;
    }
    rd["builder"] = builder;

    nlohmann::json md;
    if (!p.run_details.metadata.invocation_id.empty()) {
        md["invocationId"] = p.run_details.metadata.invocation_id;
    }
    md["startedOn"]  = to_rfc3339(p.run_details.metadata.started_on);
    md["finishedOn"] = to_rfc3339(p.run_details.metadata.finished_on);
    rd["metadata"] = md;

    if (!p.run_details.byproducts.empty()) {
        nlohmann::json bps = nlohmann::json::array();
        for (const auto& b : p.run_details.byproducts) bps.push_back(b);
        rd["byproducts"] = bps;
    }

    out["runDetails"] = rd;
    return out;
}

auto make_provenance_statement(const Subject& subject,
                               const SlsaProvenance& provenance)
    -> Statement {
    Statement s;
    s.subjects.push_back(subject);
    s.predicate_type = kSlsaProvenanceV12PredicateType;
    s.predicate      = to_json(provenance);
    return s;
}

} // namespace euxis::attest
