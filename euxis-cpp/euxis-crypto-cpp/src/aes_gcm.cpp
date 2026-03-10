#include "euxis/crypto/aes_gcm.hpp"

#include <sodium.h>

#include <cstring>

namespace euxis::crypto {

// ---------------------------------------------------------------------------
// to_base64  (defined here because it depends on libsodium)
// ---------------------------------------------------------------------------
std::string EncryptionResult::to_base64() const {
    // Serialise as  iv (12) || ciphertext (includes tag)
    const size_t raw_len = iv.size() + ciphertext.size();
    std::vector<unsigned char> raw(raw_len);
    std::memcpy(raw.data(), iv.data(), iv.size());
    std::memcpy(raw.data() + iv.size(), ciphertext.data(), ciphertext.size());

    const size_t b64_maxlen = sodium_base64_encoded_len(
        raw_len, sodium_base64_VARIANT_ORIGINAL);
    std::string b64(b64_maxlen, '\0');
    sodium_bin2base64(b64.data(), b64_maxlen, raw.data(), raw_len,
                      sodium_base64_VARIANT_ORIGINAL);
    // sodium_bin2base64 null-terminates; trim the trailing '\0'.
    b64.resize(std::strlen(b64.c_str()));
    return b64;
}

// ---------------------------------------------------------------------------
// DecryptionResult::to_string
// ---------------------------------------------------------------------------
std::string DecryptionResult::to_string() const {
    return std::string(reinterpret_cast<const char*>(plaintext.data()),
                       plaintext.size());
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
namespace {

void generate_iv(std::array<std::byte, 12>& iv) {
    randombytes_buf(iv.data(), iv.size());
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// encrypt
// ---------------------------------------------------------------------------
auto encrypt(std::span<const std::byte> data,
             std::span<const std::byte, 32> key)
    -> std::expected<EncryptionResult, CryptoError> {

    EncryptionResult result;
    generate_iv(result.iv);
    result.algorithm = "AES-256-GCM";

    // Output buffer: plaintext + crypto_aead_aes256gcm_ABYTES (16) tag
    const auto ct_len = data.size() + crypto_aead_aes256gcm_ABYTES;
    result.ciphertext.resize(ct_len);
    unsigned long long actual_ct_len = 0;

    const int rc = crypto_aead_aes256gcm_encrypt(
        reinterpret_cast<unsigned char*>(result.ciphertext.data()),
        &actual_ct_len,
        reinterpret_cast<const unsigned char*>(data.data()),
        static_cast<unsigned long long>(data.size()),
        nullptr,   // no AAD
        0,         // AAD length
        nullptr,   // nsec (unused in AES-GCM)
        reinterpret_cast<const unsigned char*>(result.iv.data()),
        reinterpret_cast<const unsigned char*>(key.data()));

    if (rc != 0) {
        return std::unexpected(CryptoError::EncryptionFailed);
    }

    result.ciphertext.resize(static_cast<size_t>(actual_ct_len));
    return result;
}

// ---------------------------------------------------------------------------
// decrypt
// ---------------------------------------------------------------------------
auto decrypt(std::span<const std::byte> ciphertext,
             std::span<const std::byte, 32> key,
             std::span<const std::byte, 12> iv)
    -> std::expected<DecryptionResult, CryptoError> {

    if (ciphertext.size() < crypto_aead_aes256gcm_ABYTES) {
        return std::unexpected(CryptoError::DecryptionFailed);
    }

    DecryptionResult result;
    const auto pt_max = ciphertext.size() - crypto_aead_aes256gcm_ABYTES;
    result.plaintext.resize(pt_max);
    unsigned long long actual_pt_len = 0;

    const int rc = crypto_aead_aes256gcm_decrypt(
        reinterpret_cast<unsigned char*>(result.plaintext.data()),
        &actual_pt_len,
        nullptr,   // nsec (unused)
        reinterpret_cast<const unsigned char*>(ciphertext.data()),
        static_cast<unsigned long long>(ciphertext.size()),
        nullptr,   // no AAD
        0,
        reinterpret_cast<const unsigned char*>(iv.data()),
        reinterpret_cast<const unsigned char*>(key.data()));

    if (rc != 0) {
        return std::unexpected(CryptoError::AuthenticationFailed);
    }

    result.plaintext.resize(static_cast<size_t>(actual_pt_len));
    result.algorithm = "AES-256-GCM";
    return result;
}

// ---------------------------------------------------------------------------
// encrypt_aad
// ---------------------------------------------------------------------------
auto encrypt_aad(std::span<const std::byte> data,
                 std::span<const std::byte, 32> key,
                 std::span<const std::byte> aad)
    -> std::expected<EncryptionResult, CryptoError> {

    EncryptionResult result;
    generate_iv(result.iv);
    result.algorithm = "AES-256-GCM";

    const auto ct_len = data.size() + crypto_aead_aes256gcm_ABYTES;
    result.ciphertext.resize(ct_len);
    unsigned long long actual_ct_len = 0;

    const int rc = crypto_aead_aes256gcm_encrypt(
        reinterpret_cast<unsigned char*>(result.ciphertext.data()),
        &actual_ct_len,
        reinterpret_cast<const unsigned char*>(data.data()),
        static_cast<unsigned long long>(data.size()),
        reinterpret_cast<const unsigned char*>(aad.data()),
        static_cast<unsigned long long>(aad.size()),
        nullptr,
        reinterpret_cast<const unsigned char*>(result.iv.data()),
        reinterpret_cast<const unsigned char*>(key.data()));

    if (rc != 0) {
        return std::unexpected(CryptoError::EncryptionFailed);
    }

    result.ciphertext.resize(static_cast<size_t>(actual_ct_len));
    return result;
}

// ---------------------------------------------------------------------------
// decrypt_aad
// ---------------------------------------------------------------------------
auto decrypt_aad(std::span<const std::byte> ciphertext,
                 std::span<const std::byte, 32> key,
                 std::span<const std::byte, 12> iv,
                 std::span<const std::byte> aad)
    -> std::expected<DecryptionResult, CryptoError> {

    if (ciphertext.size() < crypto_aead_aes256gcm_ABYTES) {
        return std::unexpected(CryptoError::DecryptionFailed);
    }

    DecryptionResult result;
    const auto pt_max = ciphertext.size() - crypto_aead_aes256gcm_ABYTES;
    result.plaintext.resize(pt_max);
    unsigned long long actual_pt_len = 0;

    const int rc = crypto_aead_aes256gcm_decrypt(
        reinterpret_cast<unsigned char*>(result.plaintext.data()),
        &actual_pt_len,
        nullptr,
        reinterpret_cast<const unsigned char*>(ciphertext.data()),
        static_cast<unsigned long long>(ciphertext.size()),
        reinterpret_cast<const unsigned char*>(aad.data()),
        static_cast<unsigned long long>(aad.size()),
        reinterpret_cast<const unsigned char*>(iv.data()),
        reinterpret_cast<const unsigned char*>(key.data()));

    if (rc != 0) {
        return std::unexpected(CryptoError::AuthenticationFailed);
    }

    result.plaintext.resize(static_cast<size_t>(actual_pt_len));
    result.algorithm = "AES-256-GCM";
    return result;
}

} // namespace euxis::crypto
