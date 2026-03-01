#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace euxis::crypto {

/// Ed25519 keypair: a 64-byte secret key (seed + public key concatenated by
/// libsodium) and the 32-byte public key.
struct Ed25519Keypair {
    std::array<std::byte, 64> secret_key;
    std::array<std::byte, 32> public_key;
};

/// Generate a new Ed25519 keypair via `crypto_sign_ed25519_keypair`.
[[nodiscard]] auto generate_keypair() -> Ed25519Keypair;

/// Create a detached Ed25519 signature over `message` using the 64-byte secret
/// key. Returns the 64-byte signature.
[[nodiscard]] auto sign(std::span<const std::byte, 64> secret_key,
                        std::span<const std::byte> message)
    -> std::array<std::byte, 64>;

/// Verify a detached Ed25519 signature over `message` with the 32-byte public
/// key. Returns true if the signature is valid.
[[nodiscard]] auto verify(std::span<const std::byte, 32> public_key,
                          std::span<const std::byte> message,
                          std::span<const std::byte, 64> signature)
    -> bool;

} // namespace euxis::crypto
