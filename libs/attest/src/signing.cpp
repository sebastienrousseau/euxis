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

namespace {

auto fail(std::string message) -> std::unexpected<SigningError> {
    return std::unexpected(SigningError{.message = std::move(message)});
}

// True iff any signature in the envelope is a valid Ed25519 signature
// over `message` by `public_key`. Malformed signatures are skipped.
auto any_signature_matches(const Envelope& envelope, std::span<const std::byte> message,
                           std::span<const std::byte, 32> public_key) -> bool {
    for (const auto& sig : envelope.signatures) {
        auto sig_dec = base64_decode(sig.sig);
        if (!sig_dec || sig_dec->size() != 64)
            continue;
        std::array<std::byte, 64> sig_arr{};
        std::memcpy(sig_arr.data(), sig_dec->data(), sig_arr.size());
        if (euxis::crypto::verify(public_key, message, sig_arr))
            return true;
    }
    return false;
}

}  // namespace

auto verify_envelope(const Envelope& envelope, std::span<const std::byte, 32> public_key)
    -> std::expected<std::string, SigningError> {
    // The signature binds payloadType through PAE, but a valid
    // signature over some other type must still not be accepted as a
    // signed in-toto Statement.
    if (envelope.payload_type != kDsseInTotoPayloadType) {
        return fail("Unexpected payloadType '" + envelope.payload_type + "' (want " +
                    kDsseInTotoPayloadType + ")");
    }
    if (envelope.signatures.empty())
        return fail("Envelope has no signatures");
    auto payload = base64_decode(envelope.payload);
    if (!payload)
        return fail("payload base64 decode: " + payload.error().message);

    const auto pae_bytes =
        pae(envelope.payload_type, std::span<const std::byte>(payload->data(), payload->size()));
    const auto message = std::as_bytes(std::span(pae_bytes));
    if (!any_signature_matches(envelope, message, public_key)) {
        return fail("No valid signature found for the supplied public key");
    }
    return std::string{reinterpret_cast<const char*>(payload->data()), payload->size()};
}

} // namespace euxis::attest
