#include "euxis/sca/cargo_lock.hpp"

#include <fstream>
#include <sstream>

namespace euxis::sca {

namespace {

auto trim(std::string_view s) -> std::string_view {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t' ||
                          s.back()  == '\r' || s.back()  == '\n')) s.remove_suffix(1);
    return s;
}

auto strip_quotes(std::string_view s) -> std::string {
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') &&
        s.front() == s.back()) {
        return std::string{s.substr(1, s.size() - 2)};
    }
    return std::string{s};
}

/// Extract the dependency name from a `dependencies = [...]` line entry.
/// Entries look like:
///   "serde 1.0.197"
///   "serde 1.0.197 (registry+https://github.com/rust-lang/crates.io-index)"
///   "serde"      (no version specifier — workspace-internal)
auto extract_dep_name(std::string_view s) -> std::string {
    s = trim(s);
    if (!s.empty() && s.back() == ',') s.remove_suffix(1);
    s = trim(s);
    auto stripped = strip_quotes(s);
    auto sp = stripped.find(' ');
    if (sp == std::string::npos) return stripped;
    return stripped.substr(0, sp);
}

} // namespace

auto parse_cargo_lock(const std::string& contents,
                      const std::filesystem::path& source)
    -> std::expected<ParsedManifest, ParseError> {
    ParsedManifest out;
    out.ecosystem  = Ecosystem::Cargo;
    out.source_file = source;

    std::istringstream stream(contents);
    std::string line;
    int line_no = 0;

    bool in_package = false;
    bool in_dependencies_array = false;
    ManifestEntry current;

    auto flush = [&]() {
        if (in_package && !current.name.empty()) {
            out.entries.push_back(std::move(current));
        }
        current = ManifestEntry{};
        in_package = false;
        in_dependencies_array = false;
    };

    while (std::getline(stream, line)) {
        ++line_no;

        // Strip comments (# ...). Conservative: only outside strings.
        if (auto hash = line.find('#'); hash != std::string::npos) {
            // Don't strip if # is inside quotes; Cargo.lock checksums
            // are hex so this is a safe heuristic.
            bool in_string = false;
            for (std::size_t i = 0; i < hash; ++i) {
                if (line[i] == '"') in_string = !in_string;
            }
            if (!in_string) line.erase(hash);
        }

        std::string_view view = trim(line);

        if (view.empty()) {
            if (in_dependencies_array) continue;  // tolerate blanks
            continue;
        }

        if (view == "[[package]]") {
            flush();
            in_package = true;
            continue;
        }

        if (!view.empty() && view.front() == '[' && view.back() == ']') {
            // Some other top-level section like [metadata] or [[patch]]
            flush();
            continue;
        }

        // Multi-line dependencies = [ ... ]
        if (in_dependencies_array) {
            if (view.starts_with("]")) {
                in_dependencies_array = false;
                continue;
            }
            current.depends_on.push_back(extract_dep_name(view));
            continue;
        }

        if (!in_package) continue;

        auto eq = view.find('=');
        if (eq == std::string::npos) continue;
        auto key   = trim(view.substr(0, eq));
        auto value = trim(view.substr(eq + 1));

        if (key == "name") {
            current.name = strip_quotes(value);
        } else if (key == "version") {
            current.version = strip_quotes(value);
        } else if (key == "source") {
            current.source = strip_quotes(value);
        } else if (key == "checksum") {
            current.hash = euxis::sbom::Hash{
                euxis::sbom::HashAlgorithm::Sha256,
                strip_quotes(value),
            };
        } else if (key == "dependencies") {
            if (value == "[]") {
                continue;  // empty array
            }
            if (value.starts_with("[")) {
                in_dependencies_array = true;
                // Single-line array?
                if (auto close = value.find(']'); close != std::string::npos) {
                    in_dependencies_array = false;
                    auto inner = value.substr(1, close - 1);
                    std::size_t pos = 0;
                    while (pos < inner.size()) {
                        auto comma = inner.find(',', pos);
                        auto end = comma == std::string::npos ? inner.size() : comma;
                        current.depends_on.push_back(
                            extract_dep_name(inner.substr(pos, end - pos)));
                        if (comma == std::string::npos) break;
                        pos = comma + 1;
                    }
                }
            }
        }
    }
    flush();

    if (out.entries.empty()) {
        return std::unexpected(ParseError{
            .message = "No [[package]] blocks found in Cargo.lock",
            .file = source,
            .line = line_no,
        });
    }

    // Heuristic: a workspace root has no `source` and tends to appear
    // first. Surface it as `root` for downstream emitters.
    for (const auto& e : out.entries) {
        if (e.source.empty()) {
            out.root = e;
            break;
        }
    }

    return out;
}

auto parse_cargo_lock_file(const std::filesystem::path& path)
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
    return parse_cargo_lock(ss.str(), path);
}

} // namespace euxis::sca
