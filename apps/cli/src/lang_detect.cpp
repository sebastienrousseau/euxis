#include "euxis/cli/lang_detect.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <unordered_map>

namespace euxis::cli {

namespace fs = std::filesystem;

auto detect_languages(const std::string& root_dir) -> LanguageProfile {
    // Function-local statics: lazy-init avoids throwing during static-init phase
    // (bugprone-throwing-static-initialization). Both maps are pure lookup tables
    // and are only consulted from within this function.
    static const std::unordered_map<std::string, std::string> ext_map = {
        {".cpp", "C++"}, {".cc", "C++"}, {".cxx", "C++"}, {".hpp", "C++"}, {".h", "C/C++"},
        {".c", "C"},
        {".rs", "Rust"},
        {".go", "Go"},
        {".py", "Python"}, {".pyi", "Python"},
        {".js", "JavaScript"}, {".mjs", "JavaScript"}, {".cjs", "JavaScript"},
        {".ts", "TypeScript"}, {".tsx", "TypeScript"},
        {".java", "Java"}, {".kt", "Kotlin"},
        {".rb", "Ruby"},
        {".swift", "Swift"},
        {".cs", "C#"},
        {".php", "PHP"},
        {".lua", "Lua"},
        {".zig", "Zig"},
        {".nim", "Nim"},
        {".ex", "Elixir"}, {".exs", "Elixir"},
        {".erl", "Erlang"},
        {".hs", "Haskell"},
        {".ml", "OCaml"}, {".mli", "OCaml"},
        {".scala", "Scala"},
        {".sh", "Shell"}, {".bash", "Shell"}, {".zsh", "Shell"}, {".fish", "Shell"},
    };

    static const std::unordered_map<std::string, std::string> framework_map = {
        {"CMakeLists.txt", "CMake"},
        {"Makefile", "Make"},
        {"Cargo.toml", "Cargo"},
        {"go.mod", "Go Modules"},
        {"package.json", "npm"},
        {"pyproject.toml", "Python (pyproject)"},
        {"setup.py", "Python (setuptools)"},
        {"requirements.txt", "pip"},
        {"Gemfile", "Bundler"},
        {"pom.xml", "Maven"},
        {"build.gradle", "Gradle"},
        {"build.gradle.kts", "Gradle (Kotlin)"},
        {"Package.swift", "Swift Package Manager"},
        {"mix.exs", "Mix"},
        {"stack.yaml", "Haskell Stack"},
        {"dune-project", "Dune"},
        {"meson.build", "Meson"},
        {"BUILD", "Bazel"},
        {"WORKSPACE", "Bazel"},
        {"flake.nix", "Nix"},
        {"vcpkg.json", "vcpkg"},
        {"conanfile.txt", "Conan"},
        {"conanfile.py", "Conan"},
        {".clang-tidy", "clang-tidy"},
        {".eslintrc.json", "ESLint"},
        {"tsconfig.json", "TypeScript"},
        {"docker-compose.yml", "Docker Compose"},
        {"Dockerfile", "Docker"},
        {".github", "GitHub Actions"},
        {".gitlab-ci.yml", "GitLab CI"},
    };

    LanguageProfile profile;
    std::unordered_map<std::string, int> lang_counts;
    std::unordered_map<std::string, bool> frameworks_seen;

    // Bounded directory scan — max 5000 entries to avoid slow repos
    int scanned = 0;
    constexpr int max_scan = 5000;
    std::error_code ec;

    for (auto it = fs::recursive_directory_iterator(root_dir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator() && scanned < max_scan;
         ++it, ++scanned) {
        if (ec) { ec.clear(); continue; }

        const auto& path = it->path();
        auto filename = path.filename().string();

        // Skip hidden dirs and common noise
        if (filename.starts_with(".") && it->is_directory()) {
            it.disable_recursion_pending();
            continue;
        }
        if (filename == "node_modules" || filename == "vendor" ||
            filename == "cmake-build" || filename == "build" || filename == "target" ||
            filename == "_deps") {
            if (it->is_directory()) it.disable_recursion_pending();
            continue;
        }

        // Framework detection from config files
        auto fw_it = framework_map.find(filename);
        if (fw_it != framework_map.end() && !frameworks_seen[fw_it->second]) {
            frameworks_seen[fw_it->second] = true;
            profile.frameworks.push_back({fw_it->second, filename});
        }

        // Language detection from extensions
        if (it->is_regular_file()) {
            auto ext = path.extension().string();
            auto lang_it = ext_map.find(ext);
            if (lang_it != ext_map.end()) {
                lang_counts[lang_it->second]++;
            }
        }
    }

    // Convert counts to sorted entries
    int total_files = 0;
    for (const auto& [lang, count] : lang_counts) total_files += count;

    for (const auto& [lang, count] : lang_counts) {
        double conf = total_files > 0 ? static_cast<double>(count) / total_files : 0.0;
        profile.languages.push_back({lang, count, conf});
    }

    // Sort by file count descending
    std::sort(profile.languages.begin(), profile.languages.end(),
              [](const auto& a, const auto& b) { return a.file_count > b.file_count; });

    if (!profile.languages.empty()) {
        profile.primary_language = profile.languages[0].name;
    }

    return profile;
}

auto LanguageProfile::to_json() const -> nlohmann::json {
    nlohmann::json j;
    j["primary_language"] = primary_language;

    nlohmann::json langs = nlohmann::json::array();
    for (const auto& l : languages) {
        langs.push_back({
            {"name", l.name},
            {"file_count", l.file_count},
            {"confidence", l.confidence}
        });
    }
    j["languages"] = langs;

    nlohmann::json fws = nlohmann::json::array();
    for (const auto& f : frameworks) {
        fws.push_back({
            {"name", f.name},
            {"config_file", f.config_file}
        });
    }
    j["frameworks"] = fws;

    return j;
}

auto LanguageProfile::summary() const -> std::string {
    std::ostringstream out;
    if (!primary_language.empty()) {
        out << primary_language;
        if (languages.size() > 1) {
            out << " (+" << (languages.size() - 1) << " more)";
        }
    } else {
        out << "unknown";
    }
    if (!frameworks.empty()) {
        out << " [";
        for (size_t i = 0; i < frameworks.size() && i < 3; ++i) {
            if (i > 0) out << ", ";
            out << frameworks[i].name;
        }
        if (frameworks.size() > 3) out << ", ...";
        out << "]";
    }
    return out.str();
}

} // namespace euxis::cli
