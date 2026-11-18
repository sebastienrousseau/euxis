#include "euxis/cli/ca_bundle.hpp"

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>
#include <string_view>

namespace euxis::cli {

namespace {

struct FakeHost {
    std::map<std::string, std::string, std::less<>> env;
    std::set<std::string, std::less<>> files;

    auto get_env() {
        return [this](const char* name) -> const char* {
            auto it = env.find(name);
            return it == env.end() ? nullptr : it->second.c_str();
        };
    }
    auto exists() {
        return [this](std::string_view path) { return files.contains(path); };
    }
    auto select() { return select_ca_bundle(get_env(), exists()); }
};

} // namespace

TEST(CaBundle, DebianBundleIsFound) {
    FakeHost host;
    host.files.insert("/etc/ssl/certs/ca-certificates.crt");
    EXPECT_EQ(host.select(), "/etc/ssl/certs/ca-certificates.crt");
}

TEST(CaBundle, FedoraBundleIsFoundWhenOpenSslDefaultsAreMissing) {
    FakeHost host;
    host.files.insert("/etc/pki/tls/certs/ca-bundle.crt");
    EXPECT_EQ(host.select(), "/etc/pki/tls/certs/ca-bundle.crt");
}

TEST(CaBundle, FirstCandidateWins) {
    FakeHost host;
    host.files.insert("/etc/ssl/cert.pem");
    host.files.insert("/etc/pki/tls/certs/ca-bundle.crt");
    EXPECT_EQ(host.select(), "/etc/pki/tls/certs/ca-bundle.crt");
}

TEST(CaBundle, UserCertFileWins) {
    FakeHost host;
    host.env["SSL_CERT_FILE"] = "/custom/ca.pem";
    host.files.insert("/etc/ssl/certs/ca-certificates.crt");
    EXPECT_EQ(host.select(), std::nullopt);
}

TEST(CaBundle, UserCertDirWins) {
    FakeHost host;
    host.env["SSL_CERT_DIR"] = "/custom/certs";
    host.files.insert("/etc/ssl/certs/ca-certificates.crt");
    EXPECT_EQ(host.select(), std::nullopt);
}

TEST(CaBundle, EmptyEnvValueIsIgnored) {
    FakeHost host;
    host.env["SSL_CERT_FILE"] = "";
    host.files.insert("/etc/ssl/cert.pem");
    EXPECT_EQ(host.select(), "/etc/ssl/cert.pem");
}

TEST(CaBundle, NoBundleLeavesOpenSslAlone) {
    FakeHost host;
    EXPECT_EQ(host.select(), std::nullopt);
}

TEST(CaBundle, CandidatesAreAbsolutePaths) {
    const auto candidates = ca_bundle_candidates();
    ASSERT_FALSE(candidates.empty());
    for (const auto path : candidates) {
        EXPECT_TRUE(path.starts_with('/')) << path;
    }
}

} // namespace euxis::cli
