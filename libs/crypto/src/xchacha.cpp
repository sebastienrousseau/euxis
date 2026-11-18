#include "euxis/crypto/xchacha.hpp"

#include <sodium.h>

#include <string>

namespace euxis::crypto {

namespace {

/// Same bound as aes_gcm.cpp (P10-R2).
constexpr std::size_t kMaxPlaintextSize = 64ULL * 1024 * 1024;

static_assert(kXChaChaNonceBytes == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
static_assert(kXChaChaTagBytes == crypto_aead_xchacha20poly1305_ietf_ABYTES);

auto as_uchar(std::span<const std::byte> s) -> const unsigned char* {
    return reinterpret_cast<const unsigned char*>(s.data());
}

auto is_zero_key(std::span<const std::byte, 32> key) -> bool {
    return sodium_is_zero(as_uchar(key), key.size()) == 1;
}

} // namespace

auto xchacha_encrypt(std::span<const std::byte> data,
                     std::span<const std::byte, 32> key,
                     std::span<const std::byte> aad)
    -> std::expected<SealedBox, CryptoError> {
    if (data.size() > kMaxPlaintextSize || is_zero_key(key)) {
        return std::unexpected(CryptoError::InvalidInput);
    }
    SealedBox box;
    randombytes_buf(box.nonce.data(), box.nonce.size());
    box.ciphertext.resize(data.size() + kXChaChaTagBytes);
    unsigned long long written = 0;
    const int rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
        reinterpret_cast<unsigned char*>(box.ciphertext.data()), &written,
        as_uchar(data), data.size(),
        aad.empty() ? nullptr : as_uchar(aad), aad.size(),
        nullptr, as_uchar(box.nonce), as_uchar(key));
    if (rc != 0) {
        return std::unexpected(CryptoError::EncryptionFailed);
    }
    box.ciphertext.resize(static_cast<std::size_t>(written));
    return box;
}

auto xchacha_decrypt(std::span<const std::byte> ciphertext,
                     std::span<const std::byte, 32> key,
                     std::span<const std::byte, kXChaChaNonceBytes> nonce,
                     std::span<const std::byte> aad)
    -> std::expected<DecryptionResult, CryptoError> {
    if (ciphertext.size() < kXChaChaTagBytes || is_zero_key(key)) {
        return std::unexpected(CryptoError::DecryptionFailed);
    }
    DecryptionResult result;
    result.plaintext.resize(ciphertext.size() - kXChaChaTagBytes);
    unsigned long long written = 0;
    const int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
        reinterpret_cast<unsigned char*>(result.plaintext.data()), &written,
        nullptr, as_uchar(ciphertext), ciphertext.size(),
        aad.empty() ? nullptr : as_uchar(aad), aad.size(),
        as_uchar(nonce), as_uchar(key));
    if (rc != 0) {
        sodium_memzero(result.plaintext.data(), result.plaintext.size());
        return std::unexpected(CryptoError::AuthenticationFailed);
    }
    result.plaintext.resize(static_cast<std::size_t>(written));
    result.algorithm = std::string{kXChaChaAlgorithm};
    return result;
}

} // namespace euxis::crypto
