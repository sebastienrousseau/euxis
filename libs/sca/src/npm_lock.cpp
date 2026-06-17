#include "euxis/sca/npm_lock.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace euxis::sca {

namespace {

/// Convert the `node_modules/...` path segment from package-lock.json
/// v3 into a (namespace, name) pair compatible with purl semantics.
///
///   "node_modules/lodash"                       -> ("",       "lodash")
///   "node_modules/@babel/core"                  -> ("@babel", "core")
///   "node_modules/x/node_modules/y"             -> ("",       "y")
///   "node_modules/x/node_modules/@scope/y"      -> ("@scope", "y")
struct NameParts {
    std::string ns;
    std::string name;
};

auto split_package_path(std::string_view path) -> NameParts {
    // Take the segment after the LAST "node_modules/" prefix, since
    // package-lock.json paths nest for dependency dedup.
    constexpr std::string_view marker = "node_modules/";
    auto pos = path.rfind(marker);
    if (pos == std::string_view::npos) return {};
    std::string_view rest = path.substr(pos + marker.size());

    if (rest.empty()) return {};
    if (rest.front() == '@') {
        auto slash = rest.find('/');
        if (slash == std::string_view::npos) return {std::string{rest}, ""};
        return {std::string{rest.substr(0, slash)},
                std::string{rest.substr(slash + 1)}};
    }
    return {"", std::string{rest}};
}

/// Best-effort integrity → euxis::sbom::Hash conversion.
/// npm `integrity` strings look like "sha512-..." or
/// "sha256-..." with base64 (NOT hex). We preserve the raw string
/// verbatim under the matching alg and set the digest as-is so
/// downstream callers can decide what to do; CycloneDX accepts hex
/// only, so the emitter will skip non-hex entries via
/// is_valid_hash().
auto parse_integrity(std::string_view integrity)
    -> std::optional<euxis::sbom::Hash> {
    auto dash = integrity.find('-');
    if (dash == std::string_view::npos) return std::nullopt;
    auto alg_str = integrity.substr(0, dash);
    auto digest  = integrity.substr(dash + 1);

    euxis::sbom::Hash h;
    h.value = std::string{digest};
    // Unknown algorithms default to Sha256 (same as the explicit
    // "sha256" branch). Kept as a separate else to document the
    // fallback intent.
    // NOLINTBEGIN(bugprone-branch-clone)
    if      (alg_str == "sha256") h.algorithm = euxis::sbom::HashAlgorithm::Sha256;
    else if (alg_str == "sha384") h.algorithm = euxis::sbom::HashAlgorithm::Sha384;
    else if (alg_str == "sha512") h.algorithm = euxis::sbom::HashAlgorithm::Sha512;
    else if (alg_str == "md5")    h.algorithm = euxis::sbom::HashAlgorithm::Md5;
    else                          h.algorithm = euxis::sbom::HashAlgorithm::Sha256;
    // NOLINTEND(bugprone-branch-clone)
    return h;
}

} // namespace

auto parse_npm_lock(const std::string& contents,
                    const std::filesystem::path& source)
    -> std::expected<ParsedManifest, ParseError> {
    ParsedManifest out;
    out.ecosystem = Ecosystem::Npm;
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

    int lockfile_version = root.value("lockfileVersion", 0);
    if (lockfile_version < 2 || lockfile_version > 3) {
        return std::unexpected(ParseError{
            .message = "Only lockfileVersion 2/3 are supported (got " +
                       std::to_string(lockfile_version) + ")",
            .file = source,
        });
    }

    auto packages_it = root.find("packages");
    if (packages_it == root.end() || !packages_it->is_object()) {
        return std::unexpected(ParseError{
            .message = "Missing or non-object `packages` member",
            .file = source,
        });
    }

    // Collect dev deps + prod deps from the root package so we can
    // mark direct vs transitive and dev-only correctly.
    auto root_pkg_it = packages_it->find("");
    std::vector<std::string> direct_prod;
    std::vector<std::string> direct_dev;
    if (root_pkg_it != packages_it->end() && root_pkg_it->is_object()) {
        ManifestEntry root_entry;
        root_entry.name    = root_pkg_it->value("name", root.value("name", std::string{}));
        root_entry.version = root_pkg_it->value("version", root.value("version", std::string{}));
        root_entry.is_direct = true;
        out.root = root_entry;

        if (auto it = root_pkg_it->find("dependencies"); it != root_pkg_it->end() && it->is_object()) {
            for (const auto& [k, _] : it->items()) direct_prod.push_back(k);
        }
        if (auto it = root_pkg_it->find("devDependencies"); it != root_pkg_it->end() && it->is_object()) {
            for (const auto& [k, _] : it->items()) direct_dev.push_back(k);
        }
    }

    auto is_direct_prod = [&](const std::string& nm) {
        return std::find(direct_prod.begin(), direct_prod.end(), nm) != direct_prod.end();
    };
    auto is_direct_dev = [&](const std::string& nm) {
        return std::find(direct_dev.begin(), direct_dev.end(), nm) != direct_dev.end();
    };

    for (const auto& [path, body] : packages_it->items()) {
        if (path.empty()) continue;            // skip root, handled above
        if (!body.is_object()) continue;

        auto parts = split_package_path(path);
        if (parts.name.empty()) continue;

        ManifestEntry e;
        e.ns = parts.ns;
        e.name = parts.name;
        e.version = body.value("version", std::string{});
        if (auto integ = body.find("integrity"); integ != body.end() && integ->is_string()) {
            e.hash = parse_integrity(integ->get<std::string>());
        }
        e.source = body.value("resolved", std::string{});
        e.is_dev = body.value("dev", false);

        std::string nm_for_root_check = parts.ns.empty()
            ? parts.name
            : parts.ns + "/" + parts.name;
        e.is_direct = is_direct_prod(nm_for_root_check) ||
                      is_direct_dev(nm_for_root_check);
        if (is_direct_dev(nm_for_root_check)) e.is_dev = true;

        if (auto deps = body.find("dependencies");
            deps != body.end() && deps->is_object()) {
            for (const auto& [k, _] : deps->items()) e.depends_on.push_back(k);
        }
        out.entries.push_back(std::move(e));
    }

    if (out.entries.empty() && !out.root) {
        return std::unexpected(ParseError{
            .message = "No packages found in package-lock.json",
            .file = source,
        });
    }
    return out;
}

auto parse_npm_lock_file(const std::filesystem::path& path)
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
    return parse_npm_lock(ss.str(), path);
}

} // namespace euxis::sca
