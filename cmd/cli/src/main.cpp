#include "euxis/cli/engine.hpp"
#include "euxis/cli/i18n.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    const char* home = std::getenv("EUXIS_HOME");
    std::string euxis_home = home ? home : "";

    // Initialize i18n from LANG/LC_ALL environment
    const char* lang_env = std::getenv("LC_ALL");
    if (!lang_env) lang_env = std::getenv("LANG");
    if (lang_env) {
        std::string locale(lang_env);
        // Extract language code (e.g. "fr" from "fr_FR.UTF-8")
        auto dot = locale.find('.');
        if (dot != std::string::npos) locale = locale.substr(0, dot);
        auto underscore = locale.find('_');
        if (underscore != std::string::npos) locale = locale.substr(0, underscore);

        auto translations_dir = std::filesystem::path(euxis_home) / "translations" / "cli";
        euxis::cli::i18n::Catalog::instance().load(locale, translations_dir);
    }

    euxis::cli::Engine engine(euxis_home);
    return engine.run(args);
}
