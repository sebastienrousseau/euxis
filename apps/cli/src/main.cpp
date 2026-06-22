#include "euxis/cli/engine.hpp"
#include "euxis/cli/i18n.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <sodium.h>

// Top-level fail-fast: if the CLI bootstrap throws (allocator failure, FS
// unwritable), terminate is the documented exit path. The C++ runtime emits
// the exception details via the OS abort handler.
// NOLINTBEGIN(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    // S6: Initialize libsodium before any crypto operations (HMAC, signing).
    // sodium_init() is idempotent: returns 0 on first call, 1 on subsequent.
    if (sodium_init() < 0) {
        std::cerr << "error: libsodium initialization failed\n";
        return 1;
    }

    std::vector<std::string> args(argv + 1, argv + argc);

    const char* home = std::getenv("EUXIS_HOME");
    std::string euxis_home;
    if (home) {
        euxis_home = home;
    } else {
        const char* user_home = std::getenv("HOME");
        if (!user_home) {
            std::cerr << "error: Neither EUXIS_HOME nor HOME is set\n";
            return 1;
        }
        euxis_home = (std::filesystem::path(user_home) / ".euxis").string();
    }

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
// NOLINTEND(bugprone-exception-escape)
