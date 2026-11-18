#include "euxis/cli/ca_bundle.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace euxis::cli {

namespace {

constexpr std::array<std::string_view, 6> kCandidates{
    "/etc/ssl/certs/ca-certificates.crt",
    "/etc/pki/tls/certs/ca-bundle.crt",
    "/etc/ssl/ca-bundle.pem",
    "/etc/pki/tls/cacert.pem",
    "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
    "/etc/ssl/cert.pem",
};

auto is_set(const char* value) -> bool { return value != nullptr && *value != '\0'; }

} // namespace

auto ca_bundle_candidates() -> std::span<const std::string_view> { return kCandidates; }

auto select_ca_bundle(const std::function<const char*(const char*)>& get_env,
                      const std::function<bool(std::string_view)>& file_exists)
    -> std::optional<std::string> {
    if (is_set(get_env("SSL_CERT_FILE")) || is_set(get_env("SSL_CERT_DIR"))) {
        return std::nullopt;
    }
    for (const auto candidate : kCandidates) {
        if (file_exists(candidate)) {
            return std::string{candidate};
        }
    }
    return std::nullopt;
}

auto ensure_ca_bundle() -> std::optional<std::string> {
#ifdef __linux__
    auto bundle = select_ca_bundle(
        [](const char* name) { return std::getenv(name); },
        [](std::string_view path) {
            std::error_code ec;
            return std::filesystem::is_regular_file(std::filesystem::path{path}, ec);
        });
    if (bundle) {
        (void)::setenv("SSL_CERT_FILE", bundle->c_str(), 0);
    }
    return bundle;
#else
    return std::nullopt;
#endif
}

} // namespace euxis::cli
