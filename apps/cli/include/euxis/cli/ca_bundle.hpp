#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace euxis::cli {

/// System CA bundle locations, most common first (the list Go's
/// crypto/x509 probes on Linux): Debian/Ubuntu/Arch/Gentoo, Fedora/RHEL,
/// openSUSE, OpenELEC, CentOS/RHEL 7+, Alpine.
[[nodiscard]] auto ca_bundle_candidates() -> std::span<const std::string_view>;

/// The CA bundle OpenSSL should use, or nullopt to leave it alone.
/// Returns nullopt when SSL_CERT_FILE or SSL_CERT_DIR is already set
/// (the user's choice wins) or when no candidate exists.
[[nodiscard]] auto select_ca_bundle(
    const std::function<const char*(const char*)>& get_env,
    const std::function<bool(std::string_view)>& file_exists) -> std::optional<std::string>;

/// On Linux, point OpenSSL at the host's CA bundle via SSL_CERT_FILE.
/// The static release binary carries Alpine's OpenSSL, whose built-in
/// paths (/etc/ssl/cert.pem, /etc/ssl/certs) miss Fedora and RHEL.
/// No-op elsewhere. Returns the bundle it selected, if any.
auto ensure_ca_bundle() -> std::optional<std::string>;

} // namespace euxis::cli
