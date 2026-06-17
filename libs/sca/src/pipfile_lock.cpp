#include "euxis/sca/pipfile_lock.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace euxis::sca {

namespace {

auto strip_eq(std::string_view v) -> std::string {
    if (v.starts_with("==")) return std::string{v.substr(2)};
    return std::string{v};
}

/// Parse one Pipfile.lock hash entry (e.g. "sha256:abcdef...") into
/// the canonical Hash struct.
auto parse_hash(std::string_view s) -> std::optional<euxis::sbom::Hash> {
    auto colon = s.find(':');
    if (colon == std::string_view::npos) return std::nullopt;
    auto alg = s.substr(0, colon);
    auto digest = s.substr(colon + 1);
    euxis::sbom::Hash h;
    h.value = std::string{digest};
    if      (alg == "sha256") h.algorithm = euxis::sbom::HashAlgorithm::Sha256;
    else if (alg == "sha384") h.algorithm = euxis::sbom::HashAlgorithm::Sha384;
    else if (alg == "sha512") h.algorithm = euxis::sbom::HashAlgorithm::Sha512;
    else if (alg == "md5")    h.algorithm = euxis::sbom::HashAlgorithm::Md5;
    else                      return std::nullopt;
    return h;
}

void absorb_section(const nlohmann::json& section,
                    bool dev,
                    std::vector<ManifestEntry>& out) {
    if (!section.is_object()) return;
    for (const auto& [name, body] : section.items()) {
        if (!body.is_object()) continue;
        ManifestEntry e;
        e.name = name;
        e.is_dev = dev;
        e.is_direct = true;  // Pipfile only records top-level deps.

        if (auto v = body.find("version"); v != body.end() && v->is_string()) {
            e.version = strip_eq(v->get<std::string>());
        }
        if (auto h = body.find("hashes"); h != body.end() && h->is_array() && !h->empty()) {
            for (const auto& hv : *h) {
                if (hv.is_string()) {
                    if (auto parsed = parse_hash(hv.get<std::string>())) {
                        e.hash = parsed;
                        break;  // keep the first valid hash
                    }
                }
            }
        }
        if (auto idx = body.find("index"); idx != body.end() && idx->is_string()) {
            e.source = idx->get<std::string>();
        }
        out.push_back(std::move(e));
    }
}

} // namespace

auto parse_pipfile_lock(const std::string& contents,
                       const std::filesystem::path& source)
    -> std::expected<ParsedManifest, ParseError> {
    ParsedManifest out;
    out.ecosystem = Ecosystem::Pypi;
    out.source_file = source;

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(contents);
    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected(ParseError{
            .message = std::string{"JSON parse: "} + e.what(),
            .file = source,
        });
    }

    if (!root.is_object()) {
        return std::unexpected(ParseError{
            .message = "Top-level JSON value is not an object",
            .file = source,
        });
    }

    if (auto def = root.find("default"); def != root.end()) {
        absorb_section(*def, /*dev=*/false, out.entries);
    }
    if (auto dev = root.find("develop"); dev != root.end()) {
        absorb_section(*dev, /*dev=*/true, out.entries);
    }

    if (out.entries.empty()) {
        return std::unexpected(ParseError{
            .message = "No default/develop sections found in Pipfile.lock",
            .file = source,
        });
    }
    return out;
}

auto parse_pipfile_lock_file(const std::filesystem::path& path)
    -> std::expected<ParsedManifest, ParseError> {
    std::ifstream f(path);
    if (!f.is_open()) {
        return std::unexpected(ParseError{
            .message = "Cannot open file",
            .file = path,
        });
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse_pipfile_lock(ss.str(), path);
}

} // namespace euxis::sca
