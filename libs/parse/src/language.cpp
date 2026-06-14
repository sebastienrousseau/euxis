#include "euxis/parse/types.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace euxis::parse {

namespace {

auto ascii_lower(std::string_view s) -> std::string {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

// Extension → Language table. Order matters: longer extensions
// (`.tpp`) are checked before shorter equivalents.
struct ExtMap {
    std::string_view ext;
    Language lang;
};
constexpr std::array<ExtMap, 14> kExtensions{{
    // C
    {"c",  Language::C},
    {"h",  Language::C},  // ambiguous with C++ headers; the heuristic below promotes if a paired .cpp exists
    // C++
    {"cpp", Language::Cpp},
    {"cxx", Language::Cpp},
    {"cc",  Language::Cpp},
    {"hpp", Language::Cpp},
    {"hxx", Language::Cpp},
    {"hh",  Language::Cpp},
    {"tpp", Language::Cpp},
    {"ipp", Language::Cpp},
    {"inl", Language::Cpp},
    {"c++", Language::Cpp},
    {"h++", Language::Cpp},
    {"cppm", Language::Cpp},  // C++ modules interface unit
}};

} // namespace

auto language_str(Language lang) noexcept -> const char* {
    switch (lang) {
        case Language::C:   return "c";
        case Language::Cpp: return "cpp";
    }
    return "unknown";
}

auto detect_language_by_extension(std::string_view ext) noexcept
    -> std::optional<Language> {
    if (!ext.empty() && ext.front() == '.') {
        ext.remove_prefix(1);
    }
    auto lower = ascii_lower(ext);
    for (const auto& entry : kExtensions) {
        if (lower == entry.ext) return entry.lang;
    }
    return std::nullopt;
}

auto detect_language(const std::filesystem::path& path) noexcept
    -> std::optional<Language> {
    auto ext = path.extension().string();
    return detect_language_by_extension(ext);
}

} // namespace euxis::parse
