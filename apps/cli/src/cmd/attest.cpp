/// @file
/// @brief Implementation of `euxis attest` and `euxis verify`.

#include "euxis/cli/cmd/attest.hpp"
#include "euxis/cli/exit_codes.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <random>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "euxis/attest/bundle.hpp"
#include "euxis/attest/dsse.hpp"
#include "euxis/attest/signing.hpp"
#include "euxis/attest/slsa.hpp"
#include "euxis/attest/statement.hpp"
#include "euxis/cache/hash.hpp"
#include "euxis/crypto/ed25519.hpp"

namespace euxis::cli::cmd {

namespace {

// ---- helpers --------------------------------------------------------------

auto read_file_bytes(const std::filesystem::path& path)
    -> std::expected<std::string, std::string> {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return std::unexpected("cannot open " + path.string());
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

auto write_file(const std::filesystem::path& path, std::string_view data,
                bool restrict_perms = false) -> bool {
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!f.good()) return false;
    f.close();
    if (restrict_perms) {
        std::error_code ec;
        std::filesystem::permissions(path,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace, ec);
    }
    return true;
}

auto random_invocation_id() -> std::string {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::uint64_t> dis;
    std::ostringstream ss;
    ss << "euxis-" << std::hex << dis(gen);
    return ss.str();
}

auto bytes_to_b64(std::span<const std::byte> data) -> std::string {
    return euxis::attest::base64_encode(data);
}

auto b64_to_bytes(std::string_view b64)
    -> std::expected<std::vector<std::byte>, std::string> {
    auto out = euxis::attest::base64_decode(b64);
    if (!out) return std::unexpected(out.error().message);
    return std::move(*out);
}

// SHA-256 via libsodium (the cache lib uses BLAKE2b; we expose SHA-256
// here so the Statement.subject.digest carries the canonical sigstore
// vocab). nlohmann_json already pulls libsodium in transitively.
//
// We dispatch through libsodium's hash function via libs/crypto helpers;
// avoiding an extra include path keeps the dep graph clean.
} // namespace

} // namespace euxis::cli::cmd

// ---- SHA-256 via libsodium (separate TU-local extern) ----------------------
#include <sodium.h>
namespace euxis::cli::cmd {
namespace {

auto sha256_hex(std::string_view data) -> std::string {
    std::array<unsigned char, crypto_hash_sha256_BYTES> out{};
    crypto_hash_sha256(out.data(),
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size());
    static constexpr std::array<char, 16> hex{
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f',
    };
    std::string s;
    s.reserve(out.size() * 2);
    for (unsigned char b : out) {
        s.push_back(hex[(b >> 4) & 0xF]);
        s.push_back(hex[b & 0xF]);
    }
    return s;
}

} // namespace

// ---- attest --------------------------------------------------------------

struct AttestArgs {
    std::filesystem::path subject_path;
    std::filesystem::path key_path;
    std::filesystem::path keygen_basename;
    std::filesystem::path bundle_path;
    std::string mode{"check"};
    std::string subject_name;
};

namespace {

auto parse_attest_args(const std::vector<std::string>& argv)
    -> std::expected<AttestArgs, std::string> {
    AttestArgs a;
    bool subj_set = false;
    for (const auto& s : argv) {
        if (s.starts_with("--mode="))         { a.mode = s.substr(7); continue; }
        if (s.starts_with("--subject-name=")) { a.subject_name = s.substr(15); continue; }
        if (s.starts_with("--key="))          { a.key_path = s.substr(6); continue; }
        if (s.starts_with("--keygen="))       { a.keygen_basename = s.substr(9); continue; }
        if (s.starts_with("--bundle="))       { a.bundle_path = s.substr(9); continue; }
        if (s == "--help" || s == "-h")       return std::unexpected("__help__");
        if (s.starts_with("-")) return std::unexpected("Unknown flag: " + s);
        if (!subj_set) { a.subject_path = s; subj_set = true; continue; }
        return std::unexpected("Unexpected positional argument: " + s);
    }
    if (a.subject_path.empty()) {
        return std::unexpected("subject file is required");
    }
    return a;
}

void print_attest_help() {
    std::println("Usage: euxis attest <subject-file> [options]");
    std::println("");
    std::println("Options:");
    std::println("  --mode=<scan-mode>          mode that produced the subject (default: check)");
    std::println("  --subject-name=<friendly>   overrides the subject's 'name' field");
    std::println("  --key=<path>                path to a base64 Ed25519 secret key (.priv)");
    std::println("  --keygen=<basename>         generate a fresh keypair to <basename>.priv/.pub");
    std::println("  --bundle=<path>             output path (default: <subject>.sigstore.json)");
}

} // namespace

int cmd_attest(Context& /*ctx*/, const std::vector<std::string>& argv) {
    auto parsed = parse_attest_args(argv);
    if (!parsed) {
        if (parsed.error() == "__help__") {
            print_attest_help();
            return to_int(ExitCode::Success);
        }
        std::println(stderr, "euxis attest: {}", parsed.error());
        return to_int(ExitCode::InfraError);
    }
    const AttestArgs& a = *parsed;

    if (!std::filesystem::exists(a.subject_path)) {
        std::println(stderr, "euxis attest: subject file does not exist: {}",
                     a.subject_path.string());
        return to_int(ExitCode::InfraError);
    }

    // Resolve key: either generate or load.
    euxis::crypto::Ed25519Keypair kp;
    if (!a.keygen_basename.empty()) {
        kp = euxis::crypto::generate_keypair();
        auto priv_path = a.keygen_basename; priv_path += ".priv";
        auto pub_path  = a.keygen_basename; pub_path  += ".pub";
        if (!write_file(priv_path,
                bytes_to_b64(std::span<const std::byte>(kp.secret_key.data(), kp.secret_key.size())),
                /*restrict_perms=*/true)) {
            std::println(stderr, "euxis attest: failed to write {}", priv_path.string());
            return to_int(ExitCode::InfraError);
        }
        if (!write_file(pub_path,
                bytes_to_b64(std::span<const std::byte>(kp.public_key.data(), kp.public_key.size())))) {
            std::println(stderr, "euxis attest: failed to write {}", pub_path.string());
            return to_int(ExitCode::InfraError);
        }
        std::println("euxis attest: wrote {} (0600) and {}",
                     priv_path.string(), pub_path.string());
    } else if (!a.key_path.empty()) {
        auto raw = read_file_bytes(a.key_path);
        if (!raw) {
            std::println(stderr, "euxis attest: read key: {}", raw.error());
            return to_int(ExitCode::InfraError);
        }
        // Strip trailing whitespace/newlines.
        std::string trimmed{*raw};
        while (!trimmed.empty() &&
               (trimmed.back() == '\n' || trimmed.back() == '\r' ||
                trimmed.back() == ' '  || trimmed.back() == '\t')) {
            trimmed.pop_back();
        }
        auto decoded = b64_to_bytes(trimmed);
        if (!decoded || decoded->size() != 64) {
            std::println(stderr,
                "euxis attest: --key must contain 64 base64-encoded Ed25519 secret bytes");
            return to_int(ExitCode::InfraError);
        }
        std::memcpy(kp.secret_key.data(), decoded->data(), 64);
        // Public key recovery: libsodium's secret key stores the
        // public key in its last 32 bytes per the Ed25519 spec.
        std::memcpy(kp.public_key.data(),
                    decoded->data() + 32, 32);
    } else {
        std::println(stderr,
            "euxis attest: provide --key=<path> or --keygen=<basename>");
        return to_int(ExitCode::InfraError);
    }

    // Build subject.
    auto subj_bytes = read_file_bytes(a.subject_path);
    if (!subj_bytes) {
        std::println(stderr, "euxis attest: read subject: {}", subj_bytes.error());
        return to_int(ExitCode::InfraError);
    }
    euxis::attest::Subject subj;
    subj.name = a.subject_name.empty()
        ? a.subject_path.filename().string()
        : a.subject_name;
    subj.digest["sha256"] = sha256_hex(*subj_bytes);
    subj.digest["blake2b-256"] = euxis::cache::hash_string(*subj_bytes);

    // Build provenance.
    euxis::attest::SlsaProvenance prov;
    prov.build_definition.external_parameters = {
        {"mode", a.mode},
        {"subject", a.subject_path.string()},
    };
    prov.build_definition.internal_parameters = {
        {"host", "euxis-cli"},
    };
    prov.run_details.metadata.invocation_id = random_invocation_id();
    prov.run_details.builder.version["euxis"] = "0.1.0";

    auto stmt = euxis::attest::make_provenance_statement(subj, prov);

    // Sign.
    auto envelope = euxis::attest::sign_statement(
        stmt, kp.secret_key,
        euxis::attest::derive_keyid(kp.public_key));
    if (!envelope) {
        std::println(stderr, "euxis attest: sign: {}", envelope.error().message);
        return to_int(ExitCode::InfraError);
    }

    // Bundle.
    euxis::attest::Bundle bundle;
    bundle.public_key.hint = euxis::attest::derive_keyid(kp.public_key);
    bundle.public_key.raw_b64 = bytes_to_b64(
        std::span<const std::byte>(kp.public_key.data(), kp.public_key.size()));
    bundle.dsse_envelope = std::move(*envelope);

    auto out_path = a.bundle_path;
    if (out_path.empty()) {
        out_path = a.subject_path;
        out_path += ".sigstore.json";
    }
    auto j = euxis::attest::to_json(bundle);
    if (!write_file(out_path, j.dump(2))) {
        std::println(stderr, "euxis attest: write bundle: {}", out_path.string());
        return to_int(ExitCode::InfraError);
    }
    std::println("euxis attest: wrote {}", out_path.string());
    std::println("  keyid:    {}", bundle.public_key.hint);
    std::println("  subject:  {}", subj.name);
    return to_int(ExitCode::Success);
}

// ---- verify ---------------------------------------------------------------

struct VerifyArgs {
    std::filesystem::path bundle_path;
    std::string key_arg;  // either a path or a base64-encoded raw key
};

namespace {

auto parse_verify_args(const std::vector<std::string>& argv)
    -> std::expected<VerifyArgs, std::string> {
    VerifyArgs a;
    bool bundle_set = false;
    for (const auto& s : argv) {
        if (s.starts_with("--key=")) { a.key_arg = s.substr(6); continue; }
        if (s == "--help" || s == "-h") return std::unexpected("__help__");
        if (s.starts_with("-")) return std::unexpected("Unknown flag: " + s);
        if (!bundle_set) { a.bundle_path = s; bundle_set = true; continue; }
        return std::unexpected("Unexpected positional argument: " + s);
    }
    if (a.bundle_path.empty()) {
        return std::unexpected("bundle path is required");
    }
    return a;
}

void print_verify_help() {
    std::println("Usage: euxis verify <bundle> [--key=<path-or-base64>]");
    std::println("");
    std::println("If --key is omitted, the public key is read from the bundle's");
    std::println("verificationMaterial.publicKey.rawBytes (if present).");
}

} // namespace

int cmd_verify(Context& /*ctx*/, const std::vector<std::string>& argv) {
    auto parsed = parse_verify_args(argv);
    if (!parsed) {
        if (parsed.error() == "__help__") {
            print_verify_help();
            return to_int(ExitCode::Success);
        }
        std::println(stderr, "euxis verify: {}", parsed.error());
        return to_int(ExitCode::InfraError);
    }
    const VerifyArgs& a = *parsed;

    auto raw = read_file_bytes(a.bundle_path);
    if (!raw) {
        std::println(stderr, "euxis verify: {}", raw.error());
        return to_int(ExitCode::InfraError);
    }
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(*raw);
    } catch (const nlohmann::json::parse_error& e) {
        std::println(stderr, "euxis verify: bundle JSON parse: {}", e.what());
        return to_int(ExitCode::InfraError);
    }
    auto bundle = euxis::attest::from_json(j);
    if (!bundle) {
        std::println(stderr, "euxis verify: bundle: {}", bundle.error().message);
        return to_int(ExitCode::InfraError);
    }

    // Resolve the public key: explicit --key wins, else the bundle's
    // own rawBytes hint.
    std::string pubkey_b64;
    if (!a.key_arg.empty()) {
        // If the value is a real file path, read it; otherwise treat
        // as inline base64.
        if (std::filesystem::exists(a.key_arg)) {
            auto file = read_file_bytes(a.key_arg);
            if (!file) {
                std::println(stderr, "euxis verify: read --key: {}", file.error());
                return to_int(ExitCode::InfraError);
            }
            pubkey_b64 = *file;
            while (!pubkey_b64.empty() &&
                   (pubkey_b64.back() == '\n' || pubkey_b64.back() == '\r' ||
                    pubkey_b64.back() == ' '  || pubkey_b64.back() == '\t')) {
                pubkey_b64.pop_back();
            }
        } else {
            pubkey_b64 = a.key_arg;
        }
    } else {
        pubkey_b64 = bundle->public_key.raw_b64;
    }
    if (pubkey_b64.empty()) {
        std::println(stderr,
            "euxis verify: no public key (use --key or include rawBytes in the bundle)");
        return to_int(ExitCode::InfraError);
    }
    auto pub_bytes = b64_to_bytes(pubkey_b64);
    if (!pub_bytes || pub_bytes->size() != 32) {
        std::println(stderr,
            "euxis verify: public key must be 32 base64-encoded Ed25519 bytes");
        return to_int(ExitCode::InfraError);
    }

    auto verified = euxis::attest::verify_envelope(
        bundle->dsse_envelope,
        std::span<const std::byte, 32>(pub_bytes->data(), 32));
    if (!verified) {
        std::println(stderr, "euxis verify: {}", verified.error().message);
        return to_int(ExitCode::PolicyViolation);
    }
    std::println("euxis verify: signature OK");
    std::println("  keyid:    {}", bundle->public_key.hint);
    std::println("  bundle:   {}", a.bundle_path.string());
    return to_int(ExitCode::Success);
}

} // namespace euxis::cli::cmd
