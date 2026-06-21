#include "euxis/slopsquatting/guard.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace euxis::slopsquatting {

namespace {

auto trim(std::string_view s) -> std::string_view {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t' ||
                          s.back()  == '\r' || s.back()  == '\n')) s.remove_suffix(1);
    return s;
}

auto parse_purl_type(std::string_view s) -> euxis::sbom::PurlType {
    if (s == "npm")     return euxis::sbom::PurlType::Npm;
    if (s == "pypi")    return euxis::sbom::PurlType::Pypi;
    if (s == "cargo")   return euxis::sbom::PurlType::Cargo;
    if (s == "golang")  return euxis::sbom::PurlType::Golang;
    if (s == "go")      return euxis::sbom::PurlType::Golang;
    if (s == "maven")   return euxis::sbom::PurlType::Maven;
    if (s == "gem")     return euxis::sbom::PurlType::Gem;
    return euxis::sbom::PurlType::Generic;
}

} // namespace

auto Guard::pypi_normalise(std::string_view s) -> std::string {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (c == '_' || c == '.') c = '-';
        out.push_back(c);
    }
    // Collapse runs of hyphens per PEP 503.
    std::string collapsed;
    collapsed.reserve(out.size());
    bool last_hyphen = false;
    for (char c : out) {
        if (c == '-') {
            if (!last_hyphen) collapsed.push_back('-');
            last_hyphen = true;
        } else {
            collapsed.push_back(c);
            last_hyphen = false;
        }
    }
    return collapsed;
}

auto Guard::bucket_for(euxis::sbom::PurlType e) -> EcosystemBucket& {
    switch (e) {
        case euxis::sbom::PurlType::Npm:    return npm_;
        case euxis::sbom::PurlType::Pypi:   return pypi_;
        case euxis::sbom::PurlType::Cargo:  return cargo_;
        case euxis::sbom::PurlType::Golang: return golang_;
        case euxis::sbom::PurlType::Maven:  return maven_;
        case euxis::sbom::PurlType::Gem:    return gem_;
        default:                            return generic_;
    }
}

auto Guard::bucket_for(euxis::sbom::PurlType e) const -> const EcosystemBucket* {
    switch (e) {
        case euxis::sbom::PurlType::Npm:    return &npm_;
        case euxis::sbom::PurlType::Pypi:   return &pypi_;
        case euxis::sbom::PurlType::Cargo:  return &cargo_;
        case euxis::sbom::PurlType::Golang: return &golang_;
        case euxis::sbom::PurlType::Maven:  return &maven_;
        case euxis::sbom::PurlType::Gem:    return &gem_;
        default:                            return &generic_;
    }
}

auto Guard::size() const noexcept -> std::size_t {
    return npm_.names.size()    + pypi_.names.size()
         + cargo_.names.size()  + golang_.names.size()
         + maven_.names.size()  + gem_.names.size()
         + generic_.names.size();
}

auto Guard::contains(std::string_view name,
                     euxis::sbom::PurlType ecosystem) const -> bool {
    const auto* b = bucket_for(ecosystem);
    if (b == nullptr) return false;

    if (ecosystem == euxis::sbom::PurlType::Pypi) {
        auto norm = pypi_normalise(name);
        return b->normalised_names.find(norm) != b->normalised_names.end();
    }
    return b->names.find(std::string{name}) != b->names.end();
}

void Guard::add(euxis::sbom::PurlType ecosystem, std::vector<std::string> names) {
    auto& bucket = bucket_for(ecosystem);
    for (auto& n : names) {
        if (n.empty()) continue;
        bucket.names.insert(n);
        if (ecosystem == euxis::sbom::PurlType::Pypi) {
            bucket.normalised_names.insert(pypi_normalise(n));
        }
    }
}

void Guard::add_with_nearest(euxis::sbom::PurlType ecosystem,
                             std::string name,
                             std::string nearest) {
    if (name.empty()) return;
    auto& bucket = bucket_for(ecosystem);
    bucket.names.insert(name);
    if (ecosystem == euxis::sbom::PurlType::Pypi) {
        bucket.normalised_names.insert(pypi_normalise(name));
    }
    if (!nearest.empty()) {
        bucket.nearest.emplace_back(std::move(name), std::move(nearest));
    }
}

namespace {

/// Parse one line of the corpus text format. Updates `eco`, `name`,
/// `nearest`; returns true when the line yielded a usable entry.
auto parse_line(std::string line,
                euxis::sbom::PurlType& eco,
                std::string& name,
                std::string& nearest) -> bool {
    line = std::string{trim(line)};
    if (line.empty() || line.front() == '#') return false;

    eco = euxis::sbom::PurlType::Generic;
    std::string body = line;

    auto colon = line.find(':');
    if (colon != std::string::npos) {
        auto prefix = std::string{trim(std::string_view{line}.substr(0, colon))};
        auto resolved = parse_purl_type(prefix);
        if (resolved != euxis::sbom::PurlType::Generic || prefix == "generic") {
            eco = resolved;
            body = std::string{trim(std::string_view{line}.substr(colon + 1))};
        }
    }

    nearest.clear();
    name = body;
    if (auto eq = body.find('='); eq != std::string::npos) {
        name    = std::string{trim(std::string_view{body}.substr(0, eq))};
        nearest = std::string{trim(std::string_view{body}.substr(eq + 1))};
    }
    return !name.empty();
}

} // namespace

auto Guard::load_corpus_file(const std::filesystem::path& path)
    -> std::expected<std::size_t, CorpusError> {
    std::ifstream f(path);
    if (!f.is_open()) {
        return std::unexpected(CorpusError{
            .message = "Cannot open file",
            .file = path,
        });
    }

    std::size_t added = 0;
    std::string raw;
    while (std::getline(f, raw)) {
        euxis::sbom::PurlType eco{};
        std::string name;
        std::string nearest;
        if (parse_line(raw, eco, name, nearest)) {
            add_with_nearest(eco, name, nearest);
            ++added;
        }
    }
    return added;
}

// Defined in seed_corpus.cpp.
extern const std::string_view kEmbeddedSeedCorpus;

auto Guard::load_seed() -> std::size_t {
    std::size_t added = 0;
    std::istringstream stream{std::string{kEmbeddedSeedCorpus}};
    std::string raw;
    while (std::getline(stream, raw)) {
        euxis::sbom::PurlType eco{};
        std::string name;
        std::string nearest;
        if (parse_line(raw, eco, name, nearest)) {
            add_with_nearest(eco, name, nearest);
            ++added;
        }
    }
    return added;
}

} // namespace euxis::slopsquatting
