#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace euxis::crypto {

/** @brief Ed25519 signing keypair. */
struct Ed25519Keypair {
    std::array<std::byte, 32> public_key;
    std::array<std::byte, 64> secret_key;
};

/** @brief Generate a new Ed25519 keypair. */
[[nodiscard]] auto generate_keypair() -> Ed25519Keypair;

/** @brief Detached signature of a message. */
[[nodiscard]] auto sign(std::span<const std::byte, 64> secret_key,
                        std::span<const std::byte> message)
    -> std::array<std::byte, 64>;

/** @brief Verify a detached signature. */
[[nodiscard]] auto verify(std::span<const std::byte, 32> public_key,
                          std::span<const std::byte> message,
                          std::span<const std::byte, 64> signature) -> bool;

} // namespace euxis::crypto
