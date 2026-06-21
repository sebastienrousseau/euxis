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
// (`.tpp`) are checked before shorter equivalents. The array
// size is intentionally left unspecified so adding or removing
// an entry can't desync from the literal initialiser — a 28-slot
// declaration with 27 initialised entries silently left a
// default-constructed `{ext="", lang=C}` slot that matched empty
// inputs and broke `LanguageDetect.UnknownReturnsNullopt`.
struct ExtMap {
    std::string_view ext;
    Language lang;
};
constexpr std::array kExtensions = std::to_array<ExtMap>({
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
    // Rust
    {"rs",  Language::Rust},
    // Go
    {"go",  Language::Go},
    // Python
    {"py",  Language::Python},
    {"pyi", Language::Python},  // type stubs
    {"pyw", Language::Python},  // Windows windowed scripts
    // JavaScript
    {"js",  Language::JavaScript},
    {"mjs", Language::JavaScript},
    {"cjs", Language::JavaScript},
    {"jsx", Language::JavaScript},  // tree-sitter-javascript natively handles JSX
    // TypeScript (just .ts variants; .tsx needs the TSX subgrammar which
    // is deferred until libs/parse links a second tree-sitter-typescript
    // entry point)
    {"ts",  Language::TypeScript},
    {"mts", Language::TypeScript},
    {"cts", Language::TypeScript},
    // Java
    {"java", Language::Java},
});

} // namespace

auto language_str(Language lang) noexcept -> const char* {
    switch (lang) {
        case Language::C:          return "c";
        case Language::Cpp:        return "cpp";
        case Language::Rust:       return "rust";
        case Language::Go:         return "go";
        case Language::Python:     return "python";
        case Language::JavaScript: return "javascript";
        case Language::TypeScript: return "typescript";
        case Language::Java:       return "java";
    }
    return "unknown";
}

auto detect_language_by_extension(std::string_view ext)
    -> std::optional<Language> {
    if (!ext.empty() && ext.front() == '.') {
        ext.remove_prefix(1);
    }
    if (ext.empty()) return std::nullopt;
    auto lower = ascii_lower(ext);
    for (const auto& entry : kExtensions) {
        if (lower == entry.ext) return entry.lang;
    }
    return std::nullopt;
}

auto detect_language(const std::filesystem::path& path)
    -> std::optional<Language> {
    auto ext = path.extension().string();
    return detect_language_by_extension(ext);
}

} // namespace euxis::parse
