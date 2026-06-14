#include "euxis/sca/manifest.hpp"

#include "euxis/sbom/purl.hpp"

namespace euxis::sca {

auto ecosystem_str(Ecosystem e) noexcept -> const char* {
    switch (e) {
        case Ecosystem::Cargo:    return "cargo";
        case Ecosystem::Npm:      return "npm";
        case Ecosystem::Pypi:     return "pypi";
        case Ecosystem::Golang:   return "golang";
        case Ecosystem::Maven:    return "maven";
        case Ecosystem::Gem:      return "gem";
        case Ecosystem::Composer: return "composer";
    }
    return "unknown";
}

namespace {

auto purl_type_for(Ecosystem e) noexcept -> euxis::sbom::PurlType {
    switch (e) {
        case Ecosystem::Cargo:    return euxis::sbom::PurlType::Cargo;
        case Ecosystem::Npm:      return euxis::sbom::PurlType::Npm;
        case Ecosystem::Pypi:     return euxis::sbom::PurlType::Pypi;
        case Ecosystem::Golang:   return euxis::sbom::PurlType::Golang;
        case Ecosystem::Maven:    return euxis::sbom::PurlType::Maven;
        case Ecosystem::Gem:      return euxis::sbom::PurlType::Gem;
        case Ecosystem::Composer: return euxis::sbom::PurlType::Composer;
    }
    return euxis::sbom::PurlType::Generic;
}

} // namespace

auto to_purl(const ManifestEntry& entry, Ecosystem ecosystem) -> std::string {
    auto purl = euxis::sbom::Purl::build(
        purl_type_for(ecosystem), entry.name, entry.version, entry.ns);
    return purl.to_string();
}

auto to_component(const ManifestEntry& entry, Ecosystem ecosystem)
    -> euxis::sbom::Component {
    euxis::sbom::Component c;
    c.name      = entry.name;
    c.version   = entry.version;
    c.purl      = to_purl(entry, ecosystem);
    c.bom_ref   = c.purl;
    c.type      = euxis::sbom::ComponentType::Library;
    c.supplier  = entry.source;
    if (entry.hash) {
        c.hashes.push_back(*entry.hash);
    }
    if (entry.is_direct) {
        c.properties["euxis:direct"] = "true";
    }
    if (entry.is_dev) {
        c.properties["euxis:dev"] = "true";
    }
    c.properties["euxis:ecosystem"] = ecosystem_str(ecosystem);
    return c;
}

} // namespace euxis::sca
