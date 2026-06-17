#include "euxis/sbom/purl.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <sstream>

namespace euxis::sbom {

namespace {

constexpr std::string_view kScheme = "pkg:";

// Per the purl spec, the segments that must be percent-encoded are
// the path components (namespace, name, version, subpath) and the
// qualifier values. The reserved characters that MUST be left alone
// are unreserved per RFC 3986 §2.3: A-Z a-z 0-9 - . _ ~
constexpr auto is_unreserved(char c) noexcept -> bool {
    return std::isalnum(static_cast<unsigned char>(c)) != 0
        || c == '-' || c == '.' || c == '_' || c == '~';
}

auto hex_char(int v) noexcept -> char {
    return v < 10 ? static_cast<char>('0' + v) : static_cast<char>('A' + (v - 10));
}

auto from_hex(char c) noexcept -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// Lowercase ASCII letters; used for the `type` segment which is
// case-insensitive per the spec.
auto ascii_lower(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace

auto purl_type_str(PurlType t) noexcept -> const char* {
    switch (t) {
        case PurlType::Npm:      return "npm";
        case PurlType::Pypi:     return "pypi";
        case PurlType::Cargo:    return "cargo";
        case PurlType::Golang:   return "golang";
        case PurlType::Maven:    return "maven";
        case PurlType::Gem:      return "gem";
        case PurlType::Nuget:    return "nuget";
        case PurlType::Composer: return "composer";
        case PurlType::Hex:      return "hex";
        case PurlType::Conan:    return "conan";
        case PurlType::Conda:    return "conda";
        case PurlType::Github:   return "github";
        case PurlType::Generic:  return "generic";
    }
    return "generic";
}

auto percent_encode_segment(const std::string& s) -> std::string {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (is_unreserved(static_cast<char>(c))) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex_char((c >> 4) & 0x0F));
            out.push_back(hex_char(c & 0x0F));
        }
    }
    return out;
}

auto percent_decode_segment(const std::string& s) -> std::string {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = from_hex(s[i + 1]);
            int lo = from_hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

auto Purl::to_string() const -> std::string {
    std::string out;
    out.reserve(64);
    out.append(kScheme);
    out.append(ascii_lower(type));
    out.push_back('/');

    if (!ns.empty()) {
        // Namespace segments are slash-separated; each path
        // component is percent-encoded individually so that
        // intentional slashes (Go modules' k8s.io/client-go) are
        // preserved.
        std::size_t start = 0;
        for (std::size_t i = 0; i <= ns.size(); ++i) {
            if (i == ns.size() || ns[i] == '/') {
                if (i > start) {
                    out.append(percent_encode_segment(ns.substr(start, i - start)));
                }
                if (i < ns.size()) out.push_back('/');
                start = i + 1;
            }
        }
        out.push_back('/');
    }

    out.append(percent_encode_segment(name));

    if (!version.empty()) {
        out.push_back('@');
        out.append(percent_encode_segment(version));
    }

    if (!qualifiers.empty()) {
        // Qualifier keys MUST be sorted alphabetically (lower-case)
        // per the purl spec. std::map orders by raw bytes, so we
        // lower-case keys *before* inserting — otherwise "arch" and
        // "Repository_URL" sort as "R" < "a" (capital-letter ASCII
        // is lower than lower-case), which violates the spec and
        // surfaces a non-canonical purl.
        std::map<std::string, std::string> sorted;
        for (const auto& [k, v] : qualifiers) {
            if (k.empty() || v.empty()) continue;
            sorted.emplace(ascii_lower(k), v);
        }
        if (!sorted.empty()) {
            out.push_back('?');
            bool first = true;
            for (const auto& [k, v] : sorted) {
                if (!first) out.push_back('&');
                out.append(k);
                out.push_back('=');
                out.append(percent_encode_segment(v));
                first = false;
            }
        }
    }

    if (!subpath.empty()) {
        out.push_back('#');
        // Per the purl spec, subpath segments are percent-encoded
        // individually but the `/` separator stays raw — same shape
        // as URL path encoding. Encoding the whole string at once
        // (the previous behaviour) escaped `/` to `%2F`.
        std::size_t start = 0;
        for (std::size_t i = 0; i <= subpath.size(); ++i) {
            if (i == subpath.size() || subpath[i] == '/') {
                if (i > start) {
                    out.append(percent_encode_segment(subpath.substr(start, i - start)));
                }
                if (i < subpath.size()) out.push_back('/');
                start = i + 1;
            }
        }
    }

    return out;
}

auto Purl::build(PurlType type, const std::string& name,
                 const std::string& version, const std::string& ns) -> Purl {
    Purl p;
    p.type = purl_type_str(type);
    p.name = name;
    p.version = version;
    p.ns = ns;
    return p;
}

auto parse_purl(const std::string& s) -> std::expected<Purl, PurlError> {
    if (s.size() < kScheme.size() ||
        s.compare(0, kScheme.size(), kScheme) != 0) {
        return std::unexpected(PurlError{
            .message = "Missing 'pkg:' scheme",
            .position = 0,
        });
    }

    std::string rest = s.substr(kScheme.size());
    while (!rest.empty() && rest.front() == '/') rest.erase(0, 1);

    auto slash = rest.find('/');
    if (slash == std::string::npos) {
        return std::unexpected(PurlError{
            .message = "purl missing type/name separator",
            .position = kScheme.size(),
        });
    }

    Purl p;
    p.type = ascii_lower(rest.substr(0, slash));
    rest = rest.substr(slash + 1);

    // Split off subpath (#…) and qualifiers (?…)
    std::string subpath;
    auto hash = rest.find('#');
    if (hash != std::string::npos) {
        subpath = rest.substr(hash + 1);
        rest = rest.substr(0, hash);
    }
    std::string qualifiers_str;
    auto q = rest.find('?');
    if (q != std::string::npos) {
        qualifiers_str = rest.substr(q + 1);
        rest = rest.substr(0, q);
    }

    // Split off version (last '@' before subpath/qualifiers)
    auto at = rest.rfind('@');
    if (at != std::string::npos) {
        p.version = percent_decode_segment(rest.substr(at + 1));
        rest = rest.substr(0, at);
    }

    // Whatever's left is namespace + name.
    auto last_slash = rest.rfind('/');
    if (last_slash == std::string::npos) {
        p.name = percent_decode_segment(rest);
    } else {
        // Decode each namespace segment individually so embedded
        // slashes inside an encoded segment are preserved.
        std::string ns_raw = rest.substr(0, last_slash);
        std::string ns_dec;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= ns_raw.size(); ++i) {
            if (i == ns_raw.size() || ns_raw[i] == '/') {
                ns_dec.append(percent_decode_segment(ns_raw.substr(start, i - start)));
                if (i < ns_raw.size()) ns_dec.push_back('/');
                start = i + 1;
            }
        }
        p.ns = ns_dec;
        p.name = percent_decode_segment(rest.substr(last_slash + 1));
    }

    // Parse qualifiers
    if (!qualifiers_str.empty()) {
        std::size_t start = 0;
        while (start < qualifiers_str.size()) {
            auto amp = qualifiers_str.find('&', start);
            std::string kv = qualifiers_str.substr(
                start, amp == std::string::npos ? std::string::npos : amp - start);
            auto eq = kv.find('=');
            if (eq != std::string::npos) {
                p.qualifiers[ascii_lower(kv.substr(0, eq))] =
                    percent_decode_segment(kv.substr(eq + 1));
            }
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }

    if (!subpath.empty()) p.subpath = percent_decode_segment(subpath);

    if (p.type.empty() || p.name.empty()) {
        return std::unexpected(PurlError{
            .message = "purl type and name must be non-empty",
            .position = 0,
        });
    }
    return p;
}

} // namespace euxis::sbom
