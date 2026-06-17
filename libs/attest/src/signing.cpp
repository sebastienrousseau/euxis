#include "euxis/attest/signing.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

#include "euxis/cache/hash.hpp"
#include "euxis/crypto/ed25519.hpp"

namespace euxis::attest {

auto derive_keyid(std::span<const std::byte, 32> public_key) -> std::string {
    return euxis::cache::hash_bytes(
        std::span<const std::byte>(public_key.data(), public_key.size()));
}

auto sign_statement(const Statement& statement,
                    std::span<const std::byte, 64> secret_key,
                    std::string keyid)
    -> std::expected<Envelope, SigningError> {
    auto valid = validate(statement);
    if (!valid) {
        return std::unexpected(SigningError{
            .message = "Statement validation: " + valid.error().message,
        });
    }

    auto payload_str = serialise_for_signing(statement);
    auto pae_bytes   = pae(kDsseInTotoPayloadType, payload_str);

    auto sig_bytes = euxis::crypto::sign(
        secret_key,
        std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(pae_bytes.data()),
            pae_bytes.size()));

    Envelope env;
    env.payload_type = kDsseInTotoPayloadType;
    env.payload      = base64_encode(payload_str);
    env.signatures.push_back(Signature{
        .keyid = std::move(keyid),
        .sig   = base64_encode(std::span<const std::byte>(
                     sig_bytes.data(), sig_bytes.size())),
    });
    return env;
}

auto verify_envelope(const Envelope& envelope,
                     std::span<const std::byte, 32> public_key)
    -> std::expected<std::string, SigningError> {
    if (envelope.signatures.empty()) {
        return std::unexpected(SigningError{
            .message = "Envelope has no signatures",
        });
    }
    auto payload_dec = base64_decode(envelope.payload);
    if (!payload_dec) {
        return std::unexpected(SigningError{
            .message = "payload base64 decode: " + payload_dec.error().message,
        });
    }
    auto pae_bytes = pae(envelope.payload_type,
        std::span<const std::byte>(payload_dec->data(), payload_dec->size()));

    bool any_ok = false;
    for (const auto& sig : envelope.signatures) {
        auto sig_dec = base64_decode(sig.sig);
        if (!sig_dec) continue;
        if (sig_dec->size() != 64) continue;
        std::array<std::byte, 64> sig_arr{};
        std::memcpy(sig_arr.data(), sig_dec->data(), 64);
        if (euxis::crypto::verify(
                public_key,
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(pae_bytes.data()),
                    pae_bytes.size()),
                std::span<const std::byte, 64>(sig_arr.data(), 64))) {
            any_ok = true;
            break;
        }
    }
    if (!any_ok) {
        return std::unexpected(SigningError{
            .message = "No valid signature found for the supplied public key",
        });
    }

    return std::string{
        reinterpret_cast<const char*>(payload_dec->data()),
        payload_dec->size(),
    };
}

} // namespace euxis::attest
