#include "euxis/sca/scanner.hpp"

#include <algorithm>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include "euxis/sca/cargo_lock.hpp"
#include "euxis/sca/go_sum.hpp"
#include "euxis/sca/npm_lock.hpp"
#include "euxis/sca/pipfile_lock.hpp"

namespace euxis::sca {

namespace {

namespace fs = std::filesystem;

auto should_skip(const fs::path& p, const ScanOptions& opts) -> bool {
    for (const auto& seg : p) {
        for (const auto& excl : opts.exclude) {
            if (seg.string() == excl) return true;
        }
    }
    return false;
}

auto dispatch(const fs::path& p)
    -> std::optional<std::expected<ParsedManifest, ParseError>> {
    const auto name = p.filename().string();
    if (name == "Cargo.lock")        return parse_cargo_lock_file(p);
    if (name == "package-lock.json") return parse_npm_lock_file(p);
    if (name == "Pipfile.lock")      return parse_pipfile_lock_file(p);
    if (name == "go.sum")            return parse_go_sum_file(p);
    return std::nullopt;
}

} // namespace

auto scan_directory(const fs::path& root, const ScanOptions& opts)
    -> std::expected<ScanResult, ParseError> {
    if (!fs::exists(root)) {
        return std::unexpected(ParseError{
            .message = "Scan target does not exist",
            .file = root,
        });
    }
    if (!fs::is_directory(root)) {
        // Single file path; dispatch directly.
        ScanResult sr;
        auto disp = dispatch(root);
        if (!disp) {
            return std::unexpected(ParseError{
                .message = "Target is not a recognised lockfile",
                .file = root,
            });
        }
        if (*disp) sr.manifests.push_back(std::move(**disp));
        else       sr.errors.push_back(disp->error());
        return sr;
    }

    ScanResult sr;
    std::error_code ec;
    fs::recursive_directory_iterator it(
        root, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        return std::unexpected(ParseError{
            .message = ec.message(),
            .file = root,
        });
    }
    fs::recursive_directory_iterator end;

    std::size_t count = 0;
    for (; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_directory(ec)) {
            if (should_skip(it->path().lexically_relative(root), opts)) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (should_skip(it->path().lexically_relative(root), opts)) continue;

        auto disp = dispatch(it->path());
        if (!disp) continue;
        if (*disp) sr.manifests.push_back(std::move(**disp));
        else       sr.errors.push_back(disp->error());

        if (++count >= opts.max_files) break;
    }

    return sr;
}

auto to_sbom_document(const ScanResult& scan, const fs::path& project_root)
    -> euxis::sbom::SbomDocument {
    euxis::sbom::SbomDocument doc;
    doc.tools.push_back({"euxis.co", "euxis-cli", "0.1.0"});

    // Root component: surface the project root path as a generic
    // application. Caller may overwrite with richer information.
    euxis::sbom::Component root;
    root.bom_ref = "euxis:scan-root";
    root.name    = project_root.filename().empty()
                   ? project_root.string()
                   : project_root.filename().string();
    root.type    = euxis::sbom::ComponentType::Application;
    doc.root = root;

    std::unordered_map<std::string, std::size_t> purl_to_index;
    std::unordered_map<std::string, std::string> name_to_purl;  // within scan

    for (const auto& m : scan.manifests) {
        // Build a (per-manifest) name → purl lookup so depends_on
        // string names resolve into the dep graph.
        std::unordered_map<std::string, std::string> local_name_to_purl;
        for (const auto& e : m.entries) {
            auto purl = to_purl(e, m.ecosystem);
            local_name_to_purl[e.name] = purl;
        }

        for (const auto& e : m.entries) {
            auto comp = to_component(e, m.ecosystem);
            auto& idx = purl_to_index[comp.purl];
            if (idx == 0 && (doc.components.empty() ||
                             doc.components.front().purl != comp.purl)) {
                doc.components.push_back(std::move(comp));
                idx = doc.components.size();  // 1-based; 0 means absent
            }
            name_to_purl[e.name] = local_name_to_purl[e.name];

            // Construct edges
            if (!e.depends_on.empty()) {
                euxis::sbom::Dependency d;
                d.ref = local_name_to_purl[e.name];
                for (const auto& dep_name : e.depends_on) {
                    auto it = local_name_to_purl.find(dep_name);
                    if (it != local_name_to_purl.end()) {
                        d.depends_on.push_back(it->second);
                    }
                }
                if (!d.depends_on.empty()) doc.dependencies.push_back(std::move(d));
            }
        }
    }

    doc.properties["euxis:scan-root"]  = project_root.string();
    doc.properties["euxis:manifest-count"] =
        std::to_string(scan.manifests.size());

    return doc;
}

} // namespace euxis::sca
