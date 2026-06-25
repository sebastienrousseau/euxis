#include "euxis/cli/main_entry.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace euxis::cli {

namespace {

class EnvSaver {
public:
    EnvSaver() {
        save("EUXIS_HOME");
        save("HOME");
        save("LANG");
        save("LC_ALL");
    }
    ~EnvSaver() {
        for (const auto& [k, v] : saved_) {
            if (v.has_value()) {
                (void)::setenv(k.c_str(), v->c_str(), 1);
            } else {
                (void)::unsetenv(k.c_str());
            }
        }
    }
    EnvSaver(const EnvSaver&) = delete;
    EnvSaver& operator=(const EnvSaver&) = delete;

private:
    void save(const std::string& k) {
        const char* v = std::getenv(k.c_str());
        saved_.emplace_back(k, v ? std::optional<std::string>{v} : std::nullopt);
    }
    std::vector<std::pair<std::string, std::optional<std::string>>> saved_;
};

// Build argv from a vector of strings; returned pointers are valid for the
// lifetime of the input vector.
std::vector<char*> make_argv(std::vector<std::string>& storage) {
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1);
    for (auto& s : storage) argv.push_back(s.data());
    argv.push_back(nullptr);
    return argv;
}

} // namespace

TEST(CliMainEntry, ExitsOneWhenNeitherEuxisHomeNorHomeSet) {
    EnvSaver saver;
    (void)::unsetenv("EUXIS_HOME");
    (void)::unsetenv("HOME");

    std::vector<std::string> args{"euxis", "--version"};
    auto argv = make_argv(args);
    int rc = cli_main(static_cast<int>(args.size()), argv.data());
    EXPECT_EQ(rc, 1);
}

TEST(CliMainEntry, VersionFlagSucceedsWithEuxisHome) {
    EnvSaver saver;
    auto tmp = std::filesystem::temp_directory_path() / "euxis-test-clihome";
    std::filesystem::create_directories(tmp);
    (void)::setenv("EUXIS_HOME", tmp.string().c_str(), 1);
    (void)::unsetenv("LANG");
    (void)::unsetenv("LC_ALL");

    std::vector<std::string> args{"euxis", "--version"};
    auto argv = make_argv(args);
    int rc = cli_main(static_cast<int>(args.size()), argv.data());
    EXPECT_EQ(rc, 0);
}

TEST(CliMainEntry, FallsBackToHomeWhenEuxisHomeUnset) {
    EnvSaver saver;
    (void)::unsetenv("EUXIS_HOME");
    auto tmp = std::filesystem::temp_directory_path() / "euxis-test-userhome";
    std::filesystem::create_directories(tmp);
    (void)::setenv("HOME", tmp.string().c_str(), 1);
    (void)::unsetenv("LANG");
    (void)::unsetenv("LC_ALL");

    std::vector<std::string> args{"euxis", "--version"};
    auto argv = make_argv(args);
    int rc = cli_main(static_cast<int>(args.size()), argv.data());
    EXPECT_EQ(rc, 0);
}

TEST(CliMainEntry, LangEnvTriggersI18nLoad) {
    EnvSaver saver;
    auto tmp = std::filesystem::temp_directory_path() / "euxis-test-i18n";
    std::filesystem::create_directories(tmp);
    (void)::setenv("EUXIS_HOME", tmp.string().c_str(), 1);
    (void)::setenv("LANG", "fr_FR.UTF-8", 1);
    (void)::unsetenv("LC_ALL");

    std::vector<std::string> args{"euxis", "--version"};
    auto argv = make_argv(args);
    int rc = cli_main(static_cast<int>(args.size()), argv.data());
    EXPECT_EQ(rc, 0);
}

TEST(CliMainEntry, LcAllOverridesLang) {
    EnvSaver saver;
    auto tmp = std::filesystem::temp_directory_path() / "euxis-test-i18n2";
    std::filesystem::create_directories(tmp);
    (void)::setenv("EUXIS_HOME", tmp.string().c_str(), 1);
    (void)::setenv("LANG", "en_US.UTF-8", 1);
    (void)::setenv("LC_ALL", "es_ES.UTF-8", 1);

    std::vector<std::string> args{"euxis", "--version"};
    auto argv = make_argv(args);
    int rc = cli_main(static_cast<int>(args.size()), argv.data());
    EXPECT_EQ(rc, 0);
}

TEST(CliMainEntry, NoArgsRunsHelpDispatch) {
    EnvSaver saver;
    auto tmp = std::filesystem::temp_directory_path() / "euxis-test-noargs";
    std::filesystem::create_directories(tmp);
    (void)::setenv("EUXIS_HOME", tmp.string().c_str(), 1);
    (void)::unsetenv("LANG");
    (void)::unsetenv("LC_ALL");

    std::vector<std::string> args{"euxis"};
    auto argv = make_argv(args);
    int rc = cli_main(static_cast<int>(args.size()), argv.data());
    EXPECT_EQ(rc, 0);
}

} // namespace euxis::cli
