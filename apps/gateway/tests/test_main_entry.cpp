#include "euxis/gateway/main_entry.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace euxis::gateway {

namespace {

class EnvSaver {
public:
    EnvSaver() {
        save("EUXIS_HOME");
        save("HOME");
    }
    ~EnvSaver() {
        for (const auto& [k, v] : saved_) {
            if (v.has_value()) (void)::setenv(k.c_str(), v->c_str(), 1);
            else (void)::unsetenv(k.c_str());
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

std::vector<char*> make_argv(std::vector<std::string>& storage) {
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1);
    for (auto& s : storage) argv.push_back(s.data());
    argv.push_back(nullptr);
    return argv;
}

// Minimum config that GatewayConfig::load_from_file accepts.
void write_min_config(const std::filesystem::path& p) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p);
    f << R"({
  "listen_address": "127.0.0.1",
  "listen_port": 0,
  "auth_token": "test-token",
  "tls_enabled": false
})";
}

} // namespace

TEST(GatewayMainEntry, ExitsOneWhenNeitherEnvSet) {
    EnvSaver saver;
    (void)::unsetenv("EUXIS_HOME");
    (void)::unsetenv("HOME");

    std::vector<std::string> args{"euxis-gateway"};
    auto argv = make_argv(args);
    int rc = gateway_main(static_cast<int>(args.size()), argv.data(), /*start_server=*/false);
    EXPECT_EQ(rc, 1);
}

TEST(GatewayMainEntry, ExitsOneWhenConfigMissing) {
    EnvSaver saver;
    auto tmp = std::filesystem::temp_directory_path() / "euxis-gw-missing";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    (void)::setenv("EUXIS_HOME", tmp.string().c_str(), 1);

    std::vector<std::string> args{"euxis-gateway"};
    auto argv = make_argv(args);
    int rc = gateway_main(static_cast<int>(args.size()), argv.data(), /*start_server=*/false);
    EXPECT_EQ(rc, 1);
}

TEST(GatewayMainEntry, ResolvesConfigFromEuxisHome) {
    EnvSaver saver;
    auto tmp = std::filesystem::temp_directory_path() / "euxis-gw-euxishome";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    write_min_config(tmp / "data" / "config" / "gateway-e2e.json");
    (void)::setenv("EUXIS_HOME", tmp.string().c_str(), 1);

    std::vector<std::string> args{"euxis-gateway"};
    auto argv = make_argv(args);
    int rc = gateway_main(static_cast<int>(args.size()), argv.data(), /*start_server=*/false);
    EXPECT_EQ(rc, 0);
}

TEST(GatewayMainEntry, ResolvesConfigFromHomeFallback) {
    EnvSaver saver;
    (void)::unsetenv("EUXIS_HOME");
    auto tmp = std::filesystem::temp_directory_path() / "euxis-gw-userhome";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    write_min_config(tmp / ".euxis" / "data" / "config" / "gateway-e2e.json");
    (void)::setenv("HOME", tmp.string().c_str(), 1);

    std::vector<std::string> args{"euxis-gateway"};
    auto argv = make_argv(args);
    int rc = gateway_main(static_cast<int>(args.size()), argv.data(), /*start_server=*/false);
    EXPECT_EQ(rc, 0);
}

TEST(GatewayMainEntry, AcceptsExplicitConfigPathArgv) {
    EnvSaver saver;
    (void)::unsetenv("EUXIS_HOME");
    (void)::unsetenv("HOME");
    auto cfg = std::filesystem::temp_directory_path() / "euxis-gw-explicit.json";
    write_min_config(cfg);

    std::vector<std::string> args{"euxis-gateway", cfg.string()};
    auto argv = make_argv(args);
    int rc = gateway_main(static_cast<int>(args.size()), argv.data(), /*start_server=*/false);
    EXPECT_EQ(rc, 0);
}

} // namespace euxis::gateway
