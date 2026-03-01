#include "euxis/crypto/ed25519.hpp"

#include <sodium.h>

#include <cstring>

namespace euxis::crypto {

auto generate_keypair() -> Ed25519Keypair {
    Ed25519Keypair kp{};
    crypto_sign_ed25519_keypair(
        reinterpret_cast<unsigned char*>(kp.public_key.data()),
        reinterpret_cast<unsigned char*>(kp.secret_key.data()));
    return kp;
}

auto sign(std::span<const std::byte, 64> secret_key,
          std::span<const std::byte> message)
    -> std::array<std::byte, 64> {

    std::array<std::byte, 64> sig{};
    unsigned long long sig_len = 0;

    crypto_sign_ed25519_detached(
        reinterpret_cast<unsigned char*>(sig.data()),
        &sig_len,
        reinterpret_cast<const unsigned char*>(message.data()),
        static_cast<unsigned long long>(message.size()),
        reinterpret_cast<const unsigned char*>(secret_key.data()));

    return sig;
}

auto verify(std::span<const std::byte, 32> public_key,
            std::span<const std::byte> message,
            std::span<const std::byte, 64> signature)
    -> bool {

    const int rc = crypto_sign_ed25519_verify_detached(
        reinterpret_cast<const unsigned char*>(signature.data()),
        reinterpret_cast<const unsigned char*>(message.data()),
        static_cast<unsigned long long>(message.size()),
        reinterpret_cast<const unsigned char*>(public_key.data()));

    return rc == 0;
}

} // namespace euxis::crypto
