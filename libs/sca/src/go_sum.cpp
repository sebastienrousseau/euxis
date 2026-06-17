#include "euxis/sca/go_sum.hpp"

#include <fstream>
#include <map>
#include <sstream>

namespace euxis::sca {

namespace {

/// Split a Go module path into (purl namespace, package name).
///
///   "k8s.io/client-go"             -> ("k8s.io",            "client-go")
///   "github.com/foo/bar/sub"       -> ("github.com/foo/bar","sub")
///   "rsc.io/quote"                 -> ("rsc.io",            "quote")
///   "go.uber.org/zap"              -> ("go.uber.org",       "zap")
struct ModuleParts {
    std::string ns;
    std::string name;
};

auto split_module(std::string_view module_path) -> ModuleParts {
    auto last = module_path.rfind('/');
    if (last == std::string_view::npos) {
        return {"", std::string{module_path}};
    }
    return {std::string{module_path.substr(0, last)},
            std::string{module_path.substr(last + 1)}};
}

auto trim(std::string_view s) -> std::string_view {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t' ||
                          s.back()  == '\r' || s.back()  == '\n')) s.remove_suffix(1);
    return s;
}

} // namespace

auto parse_go_sum(const std::string& contents,
                  const std::filesystem::path& source)
    -> std::expected<ParsedManifest, ParseError> {
    ParsedManifest out;
    out.ecosystem = Ecosystem::Golang;
    out.source_file = source;

    // Map keyed on "module@version" to dedupe the two-line shape.
    // We keep insertion order so the resulting Components reflect the
    // file's ordering for stable evidence packs.
    std::vector<std::string> order;
    std::map<std::string, ManifestEntry> dedup;

    std::istringstream stream(contents);
    std::string raw;
    int line_no = 0;

    while (std::getline(stream, raw)) {
        ++line_no;
        auto line = trim(raw);
        if (line.empty() || line.front() == '#') continue;

        // Tokenise on whitespace into at most 3 fields.
        std::vector<std::string> toks;
        std::size_t i = 0;
        while (i < line.size() && toks.size() < 3) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            std::size_t j = i;
            while (j < line.size() && line[j] != ' ' && line[j] != '\t') ++j;
            if (j > i) toks.emplace_back(line.substr(i, j - i));
            i = j;
        }
        if (toks.size() != 3) continue;  // skip malformed lines

        const std::string& module    = toks[0];
        std::string version_token    = toks[1];
        const std::string& hash_str  = toks[2];

        bool is_go_mod = false;
        constexpr std::string_view tail = "/go.mod";
        if (version_token.size() >= tail.size() &&
            version_token.compare(version_token.size() - tail.size(),
                                   tail.size(), tail) == 0) {
            version_token.resize(version_token.size() - tail.size());
            is_go_mod = true;
        }

        std::string key = module + "@" + version_token;
        auto it = dedup.find(key);
        if (it == dedup.end()) {
            auto parts = split_module(module);
            ManifestEntry e;
            e.ns      = parts.ns;
            e.name    = parts.name;
            e.version = version_token;
            // The canonical zip hash uses h1:<base64-sha256>. We
            // record the base64 string verbatim and tag it as
            // Sha256; downstream CycloneDX consumers will skip
            // non-hex digests via is_valid_hash() (which is the
            // desired behaviour — Go uses base64, not hex).
            if (hash_str.starts_with("h1:")) {
                e.hash = euxis::sbom::Hash{
                    euxis::sbom::HashAlgorithm::Sha256,
                    hash_str.substr(3),
                };
            }
            order.push_back(key);
            dedup.emplace(key, std::move(e));
        } else if (!is_go_mod) {
            // Prefer the non-go.mod h1 hash on collision.
            if (hash_str.starts_with("h1:")) {
                it->second.hash = euxis::sbom::Hash{
                    euxis::sbom::HashAlgorithm::Sha256,
                    hash_str.substr(3),
                };
            }
        }
    }

    for (const auto& key : order) {
        out.entries.push_back(dedup[key]);
    }

    if (out.entries.empty()) {
        return std::unexpected(ParseError{
            .message = "No module entries found in go.sum",
            .file = source,
            .line = line_no,
        });
    }
    return out;
}

auto parse_go_sum_file(const std::filesystem::path& path)
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
    return parse_go_sum(ss.str(), path);
}

} // namespace euxis::sca
